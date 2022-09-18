#include "open_spiel/spiel_utils.h"

#include "open_spiel/games/twixt.h"
#include "open_spiel/games/twixt/twixtboard.h"
#include "open_spiel/utils/tensor_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <iostream>

namespace open_spiel {
namespace twixt {
namespace {

// Facts about the game.
const GameType kGameType {
		/*short_name=*/"twixt",
		/*long_name=*/"TwixT", 
		GameType::Dynamics::kSequential,
		GameType::ChanceMode::kDeterministic,
		GameType::Information::kPerfectInformation, 
		GameType::Utility::kZeroSum,
		GameType::RewardModel::kTerminal,
		/*max_num_players=*/2,
		/*min_num_players=*/2,
		/*provides_information_state_string=*/true,
		/*provides_information_state_tensor=*/false,
		/*provides_observation_string=*/true,
		/*provides_observation_tensor=*/true,
		/*parameter_specification=*/
		{
		  { "board_size", GameParameter(kDefaultBoardSize) },
		  { "ansi_color_output", GameParameter(kDefaultAnsiColorOutput) },
		  { "discount",	GameParameter(kDefaultDiscount)    }
		},
};


std::unique_ptr<Game> Factory(const GameParameters &params) {
	return std::unique_ptr < Game > (new TwixTGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

}  // namespace

TwixTState::TwixTState(std::shared_ptr<const Game> game) :   State(game) {
	const TwixTGame &parent_game = static_cast<const TwixTGame&>(*game);
	mBoard = Board(
		parent_game.getBoardSize(),
		parent_game.getAnsiColorOutput()
	);

}

void TwixTState::ObservationTensor (open_spiel::Player player, absl::Span<float> values) const {

	SPIEL_CHECK_GE(player, 0);
	SPIEL_CHECK_LT(player, kMaxPlayer);

	// 10 2-dim boards: 
	int size = mBoard.getSize();
	TensorView<3> view(values, {2 * (1 + 4), size-2, size}, true);

	// There are 10 planes: 
	// current player: plane 0: pegs, 1: NNE-links, 2: ENE-links, 3: ESE-links, 4: SSE-links
	// opponent: plane 5: pegs, 6: NNE-links, 7: ENE-links, 8: ESE-links, 9: SSE-links
	// player 0: we ignore blue end lines, i.e. we look only at cells in column #1 to column #size-1
	// player 1: we ignore red end lines, i.e. we look only at cells in row #1 to row #size-1
	// player 1's coords are turned 90 deg clockwise to fit the shape (size-2, size)

	int redOffset = 0;
	int blueOffset = 5;

	for (int c = 0; c < size; c++) {
		for (int r = 0; r < size; r++) {
			Tuple t = { c, r };
			const Cell *pCell = mBoard.getConstCell(t); 
			int color = pCell->getColor();
			if (color == kRedColor) {
				// no turn
				view[{0+redOffset, c-1, r}] = 1.0;
				if (pCell->hasLinks()) {
					if (pCell->hasLink(kNNE)) { view[{1+redOffset, c-1, r}] = 1.0; };
					if (pCell->hasLink(kENE)) { view[{2+redOffset, c-1, r}] = 1.0; };
					if (pCell->hasLink(kESE)) { view[{3+redOffset, c-1, r}] = 1.0; };
					if (pCell->hasLink(kSSE)) { view[{4+redOffset, c-1, r}] = 1.0; };
				}
			} else if (color == kBlueColor) {
				// 90 deg clockwise turn
				view[{0+blueOffset, r-1, size-c-1}] = 1.0;
				if (pCell->hasLinks()) {
					if (pCell->hasLink(kNNE)) { view[{1+blueOffset, r-1, size-c-1}] = 1.0; };
					if (pCell->hasLink(kENE)) { view[{2+blueOffset, r-1, size-c-1}] = 1.0; };
					if (pCell->hasLink(kESE)) { view[{3+blueOffset, r-1, size-c-1}] = 1.0; };
					if (pCell->hasLink(kSSE)) { view[{4+blueOffset, r-1, size-c-1}] = 1.0; };
				}
			}
		}
	}


};

TwixTGame::TwixTGame(const GameParameters &params) :
		Game(kGameType, params),
		mAnsiColorOutput(
				ParameterValue<bool>("ansi_color_output",kDefaultAnsiColorOutput)
		),
		mBoardSize(
				ParameterValue<int>("board_size", kDefaultBoardSize)
		),
		mDiscount(
				ParameterValue<double>("discount", kDefaultDiscount)
		) {
	if (mBoardSize < kMinBoardSize || mBoardSize > kMaxBoardSize) {
		SpielFatalError(
				"board_size out of range [" + std::to_string(kMinBoardSize) + ".."
						+ std::to_string(kMaxBoardSize) + "]: "
						+ std::to_string(mBoardSize) + "; ");
	}

	if (mDiscount <= kMinDiscount || mDiscount > kMaxDiscount) {
		SpielFatalError(
				"discount out of range [" + std::to_string(kMinDiscount)
						+ " < discount <= " + std::to_string(kMaxDiscount) + "]: "
						+ std::to_string(mDiscount) + "; ");
	}
}

}  // namespace twixt
}  // namespace open_spiel
