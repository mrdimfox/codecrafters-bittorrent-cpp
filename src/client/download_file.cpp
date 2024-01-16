#include "download_file.hpp"

#include <algorithm>
#include <array>
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
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include <magic_enum.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <spdlog/spdlog.h>

#include "client/piece.hpp"
#include "indicators/color.hpp"
#include "indicators/setting.hpp"
#include "misc/parse_ip_port.hpp"
#include "torrent.hpp"

namespace torrent::client {

#define OUT

namespace ind = indicators;

using Workers = std::vector<std::unique_ptr<client::PieceWorker>>;
using Indexes = std::list<std::size_t>;
using MultiprogressBar = ind::DynamicProgress<ind::ProgressBar>;
using MultiprogressBarShared = std::shared_ptr<MultiprogressBar>;
using ProgressBarsShared = std::vector<std::shared_ptr<ind::ProgressBar>>;


constexpr static auto DEFAULT_BAR_COLOR = ind::Color::grey;

auto progress_bar(ind::Color color = DEFAULT_BAR_COLOR)
  -> indicators::ProgressBar
{
    using namespace indicators;

    return ProgressBar{
      option::BarWidth{50},
      option::Start{"["},
      option::Fill{"■"},
      option::Lead{"■"},
      option::Remainder{"-"},
      option::End{" ]"},
      option::PostfixText{"..."},
      option::ForegroundColor{color},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
      option::ShowElapsedTime{true},
    };
}

auto progress_bar_shared(ind::Color color = DEFAULT_BAR_COLOR)
  -> std::shared_ptr<indicators::ProgressBar>
{
    using namespace indicators;

    return std::make_shared<ProgressBar>(
      option::BarWidth{50}, option::Start{"["}, option::Fill{"■"},
      option::Lead{"■"}, option::Remainder{"-"}, option::End{" ]"},
      option::PostfixText{"..."}, option::ForegroundColor{color},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
      option::ShowElapsedTime{true}
    );
}

auto set_progress(
  indicators::ProgressBar& bar,
  std::size_t current,
  std::size_t max,
  std::string prefix
)
{
#ifdef NDEBUG
    const auto msg = fmt::format("{} {}/{}", prefix, current, max);
    bar.set_option(indicators::option::PostfixText{msg});

    const auto progress = long((double(current) / max) * 100.0);
    bar.set_progress(progress);
#endif
}

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

void pop_piece(Indexes& pieces_to_download, std::size_t idx)
{
    auto piece = std::ranges::find(pieces_to_download, idx);

    if (piece != pieces_to_download.end()) {
        pieces_to_download.erase(piece);
    }
    else {
        throw std::runtime_error(
          fmt::format("Can download piece twice! Bad piece: {}", idx)
        );
    }
}

void setup_workers(
  const std::vector<std::string>& peers,
  const Metainfo& meta,
  OUT Workers& workers,
  OUT std::shared_ptr<MultiprogressBar> per_pieces_progress_bar,
  OUT ProgressBarsShared& bars
)
{
    const auto peers_count = peers.size();

    fmt::println("{}", "Setup connection...");

    auto bar = progress_bar(indicators::Color::blue);

    for (auto i_peer = 0; i_peer < peers_count; i_peer++) {
        auto [peer_ip, peer_port] = utils::parse_ip_port(peers[i_peer]);

        auto worker_bar = progress_bar_shared();
        auto i_bar = bars.size();

        auto worker = std::make_unique<client::PieceWorker>(
          meta, peer_ip, std::stoull(peer_port),
          [per_pieces_progress_bar, i_bar](
            std::size_t idx, std::size_t downloaded, std::size_t overall
          ) {
              auto& bar = (*per_pieces_progress_bar)[i_bar];
              set_progress(
                bar, downloaded, overall, fmt::format("Piece {}", idx)
              );
          }
        );

        set_progress(bar, i_peer + 1, peers_count, "Peers checked");

        worker->check_connection_async();
        if (worker->wait_connection_established()) {
            workers.push_back(std::move(worker));

            per_pieces_progress_bar->push_back(*worker_bar);
            bars.push_back(std::move(worker_bar));

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

auto write_piece(
  const std::filesystem::path& pieces_files_path,
  std::string_view piece,
  std::size_t piece_idx
)
{
    std::fstream piece_file;

    piece_file.open(
      pieces_files_path / fmt::format("piece_{}", piece_idx),
      std::ios::out | std::ios::binary
    );

    piece_file.write(piece.data(), piece.size());
}

auto gather_pieces(
  std::size_t pieces_count,
  const std::filesystem::path& pieces_files_path,
  std::ostream& output_stream
)
{
    std::array<char, 512> io_buffer;

    for (std::size_t piece = 0; piece < pieces_count; piece++) {
        const auto piece_path =
          pieces_files_path / fmt::format("piece_{}", piece);

        if (not std::filesystem::exists(piece_path)) {
            throw std::runtime_error(fmt::format(
              "Gathering file error. Piece {} not found.", piece_path.c_str()
            ));
        }

        std::fstream piece_file;
        piece_file.open(piece_path, std::ios::in | std::ios::binary);

        std::streamsize read_size = -1;
        do {
            read_size = piece_file.readsome(io_buffer.data(), io_buffer.size());
            output_stream.write(io_buffer.data(), read_size);
        } while (read_size != 0);
    }
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

    Indexes pieces_to_download = shuffled_indexes(pieces_count);

    Workers workers;

    auto per_pieces_progress_bar = std::make_shared<MultiprogressBar>();
#ifdef NDEBUG
    per_pieces_progress_bar->set_option(  //
      ind::option::HideBarWhenComplete{false}
    );
#else
    per_pieces_progress_bar->set_option(  //
      ind::option::HideBarWhenComplete{true}
    );
#endif

    ProgressBarsShared bars;

    setup_workers(
      peers, meta, OUT workers, OUT per_pieces_progress_bar, OUT bars
    );

    if (workers.empty()) {
        throw std::runtime_error("No peers available!");
    }

    const auto pieces_files_path =
      std::filesystem::current_path().append(".pieces");

    create_dir_for_pieces(pieces_files_path);

    std::map<size_t, std::fstream> pieces_files;

    size_t pieces_count_requested = 0;

    auto download_progress_bar = progress_bar(ind::Color::green);
    download_progress_bar.set_option(ind::option::ShowPercentage{true});
    per_pieces_progress_bar->push_back(download_progress_bar);

    fmt::println("Downloading...");

    while (not pieces_to_download.empty()) {
        start_workers(pieces_to_download, workers);

        auto started_workers = workers |  //
                               ranges::views::filter(&PieceWorker::started);

        for (auto&& w : started_workers) {
            if (not w->wait_piece_transfer()) {
                if (not w->have_mode()) {
                    w->raise();
                }
                else {
                    continue;
                }
            }

            pieces_count_requested += 1;
            set_progress(
              (*per_pieces_progress_bar)[bars.size()],
              pieces_count_requested + 1, pieces_count, "Pieces received"
            );

            const std::size_t piece_idx = w->last_piece_idx();
            pop_piece(pieces_to_download, piece_idx);
            write_piece(pieces_files_path, w->piece(), piece_idx);
            bytes_received += w->piece().size();

            spdlog::debug(
              "Piece {} received: {} bytes", piece_idx, w->piece().size()
            );
        }
    }

    gather_pieces(pieces_count, pieces_files_path, ostream);

    spdlog::debug("Overall bytes received {}/{}", bytes_received, meta.length);
}

}  // namespace torrent::client
