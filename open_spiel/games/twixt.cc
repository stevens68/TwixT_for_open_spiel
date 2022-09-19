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

	int size = mBoard.getSize();
	TensorView<3> view(values, {kNumPlanes, size, size}, true);

	// 6 planes boardSize x boardSize: 
	// plane 0: red pegs, 
	// plane 1: blue pegs,
	// to avoid redundany. we only store the links with eastern direction for both colors in one plane
	// plane 2: NNE-links, 
	// plane 3: ENE-links, 
	// plane 4: ESE-links, 
	// plane 5: SSE-links
	// plane 6: player-plane

	for (int c = 0; c < size; c++) {
		for (int r = 0; r < size; r++) {
			Tuple t = { c, r };
			const Cell *pCell = mBoard.getConstCell(t); 
			int color = pCell->getColor();
			view[{6, c, r}] = (float) player;
			if (color == kRedColor || color == kBlueColor) {
				// there's a peg
				view[{color, c, r}] = 1.0;
				if (pCell->hasLinks()) {
					if (pCell->hasLink(kNNE)) { view[{2, c, r}] = 1.0; };
					if (pCell->hasLink(kENE)) { view[{3, c, r}] = 1.0; };
					if (pCell->hasLink(kESE)) { view[{4, c, r}] = 1.0; };
					if (pCell->hasLink(kSSE)) { view[{5, c, r}] = 1.0; };
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
