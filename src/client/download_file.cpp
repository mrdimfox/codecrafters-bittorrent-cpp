#include "download_file.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <magic_enum.hpp>
#include <range/v3/algorithm/count.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/zip.hpp>
#include <spdlog/spdlog.h>

#include "client/piece.hpp"
#include "misc/parse_ip_port.hpp"
#include "range/v3/view/for_each.hpp"
#include "torrent.hpp"

namespace torrent::client {

#define OUT

using Workers = std::vector<std::unique_ptr<client::PieceWorker>>;
using Indexes = std::list<std::size_t>;

Indexes shuffled_indexes(std::size_t up_to_number)
{
    std::mt19937 gen;
    std::vector<std::size_t> shuffled_pieces =
      ranges::views::ints(size_t(0), up_to_number) |
      ranges::to<std::vector<std::size_t>>();

    std::ranges::shuffle(shuffled_pieces, gen);

    return Indexes(shuffled_pieces.begin(), shuffled_pieces.end());
}

auto get_some_indexes(const Indexes& indexes, std::size_t up_to)
  -> ranges::take_view<ranges::ref_view<const Indexes>>
{
    if (indexes.empty()) {
        return {};
    }

    const auto slice =
      indexes | ranges::views::take(std::min(up_to, indexes.size()));

    return slice;
}

void setup_workers(
  const std::vector<std::string>& peers,
  const Metainfo& meta,
  OUT Workers& workers
)
{
    const auto peers_count = peers.size();

    for (auto i_peer = 0; i_peer < peers_count; i_peer++) {
        auto [peer_ip, peer_port] = utils::parse_ip_port(peers[i_peer]);

        auto worker = std::make_unique<client::PieceWorker>(
          meta, peer_ip, std::stoull(peer_port)
        );

        worker->check_connection_async();
        if (worker->wait_connection_established()) {
            workers.push_back(std::move(worker));
            spdlog::debug(
              "Set worker {} to communicate with peer {}", i_peer, peers[i_peer]
            );
        }
    }
}

void create_dir_for_pieces(const std::filesystem::path& pieces_files_path)
{
    if (not std::filesystem::exists(pieces_files_path)) {
        if (!std::filesystem::create_directories(pieces_files_path)) {
            throw std::runtime_error(fmt::format(
              "Can not create path {} for temp files", pieces_files_path.c_str()
            ));
        }
    }
}

auto start_workers(const Indexes& pieces_to_download, const Workers& workers)
  -> std::size_t
{
    auto available_workers_count =
      std::ranges::count_if(workers, [&](auto& w) { return not w->started(); });

    auto available_workers =
      workers |  //
      ranges::views::filter([](auto&& w) { return not w->started(); });

    auto indexes =
      get_some_indexes(pieces_to_download, available_workers_count);

    std::size_t count = 0;
    for (auto&& [i, w] : ranges::views::zip(indexes, available_workers)) {
        w->download_piece_async(i);
        spdlog::info("Request piece {}", i);
        count++;
    }

    return count;
}

auto download_file(
  const Metainfo& meta,
  const std::vector<std::string> peers,
  std::basic_ostream<char>& ostream
) -> void
{
    using namespace std::chrono_literals;

    auto bytes_received = 0;
    const std::size_t pieces_count = meta.pieces().size();
    const auto peers_count = peers.size();

    std::list<std::size_t> pieces_to_download = shuffled_indexes(pieces_count);
    std::vector<std::unique_ptr<client::PieceWorker>> workers;

    setup_workers(peers, meta, workers);

    if (workers.empty()) {
        throw std::runtime_error("No peers available!");
    }

    const auto pieces_files_path =
      std::filesystem::current_path().append(".pieces");

    create_dir_for_pieces(pieces_files_path);

    std::map<size_t, std::fstream> pieces_files;

    size_t pieces_count_requested = 0;

    while (not pieces_to_download.empty()) {
        pieces_count_requested += start_workers(pieces_to_download, workers);;

        auto started_workers = workers |  //
                               ranges::views::filter(&PieceWorker::started);

        for (auto&& w : started_workers) {
            if (not w->wait_piece_transfer()) {
                w->raise();
            }

            auto piece =
              std::ranges::find(pieces_to_download, w->last_piece_idx());

            if (piece != pieces_to_download.end()) {
                pieces_to_download.erase(piece);
            }
            else {
                throw std::runtime_error(fmt::format(
                  "Can download piece twice! Bad piece: {}", w->last_piece_idx()
                ));
            }

            std::fstream piece_file;
            piece_file.open(
              pieces_files_path / fmt::format("piece_{}", w->last_piece_idx()),
              std::ios::out | std::ios::binary
            );
            piece_file.write(w->piece().data(), w->piece().size());

            bytes_received += w->piece().size();

            spdlog::debug(
              "Piece {} received: {} bytes", w->last_piece_idx(),
              w->piece().size()
            );

            spdlog::info(
              "Piece {}/{} received", pieces_count_requested, pieces_count
            );
        }
    }

    spdlog::debug("Overall bytes received {}/{}", bytes_received, meta.length);
}

}  // namespace torrent::client
