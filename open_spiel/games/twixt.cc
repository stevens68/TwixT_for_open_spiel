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

std::string TwixTState::ActionToString(open_spiel::Player player, Action action) const
{
	Move move = mBoard.actionToMove(player, action);	
	std::string s = (player == kRedPlayer) ? "x" : "o";
	s += char(int('a') + move.first);
	s.append(std::to_string(mBoard.getSize() - move.second));
	return s;

};

void TwixTState::setPegAndLinksOnTensor(absl::Span<float> values, const Cell *pCell, int offset, int col, int row) const {
	// we flip col/row here for better output in playthrough file 
	TensorView<3> view(values, {kNumPlanes, mBoard.getSize(), mBoard.getSize()-2}, false);
	view[{0 + offset, row, col}] = 1.0;
	if (pCell->hasLinks()) {
		if (pCell->hasLink(kNNE)) { view[{1 + offset, row, col}] = 1.0; };
		if (pCell->hasLink(kENE)) { view[{2 + offset, row, col}] = 1.0; };
		if (pCell->hasLink(kESE)) { view[{3 + offset, row, col}] = 1.0; };
		if (pCell->hasLink(kSSE)) { view[{4 + offset, row, col}] = 1.0; };
	}
}


void TwixTState::ObservationTensor (open_spiel::Player player, absl::Span<float> values) const {

	SPIEL_CHECK_GE(player, 0);
	SPIEL_CHECK_LT(player, kNumPlayers);

	int size = mBoard.getSize();

	// 10 planes of size boardSize x (boardSize-2): 
	// each plane excludes the endlines of the opponent
	// planes 0-4 are for the current player from current player's perspective)
	// planes 5-9 are for the opponent from current player's perspective 
	// plane 0/5: pegs, 
	// plane 1/6: NNE-links, 
	// plane 2/7: ENE-links, 
	// plane 3/8: ESE-links, 
	// plane 4/9: SSE-links

	TensorView<3> view(values, {kNumPlanes, mBoard.getSize(), mBoard.getSize()-2}, true);

	for (int c = 0; c < size; c++) {
		for (int r = 0; r < size; r++) {
			Move move = { c, r };
			const Cell *pCell = mBoard.getConstCell(move); 
			int color = pCell->getColor();
			if (player == kRedPlayer) {
				if (color == kRedColor) {
					// no turn
					setPegAndLinksOnTensor(values, pCell, 0, c-1, r);
				} else if (color == kBlueColor) {
					// 90 degr turn (blue player sits left side of red player)
					setPegAndLinksOnTensor(values, pCell, 5, size-r-2, c);
				}
			} else if (player == kBluePlayer) {
				if (color == kBlueColor) {
					// 90 degr turn 
					setPegAndLinksOnTensor(values, pCell, 0, size-r-2, c);
				} else if (color == kRedColor) {
					// 90+90 degr turn (red player sits left of blue player)
					setPegAndLinksOnTensor(values, pCell, 5, size-c-2, size-r-1);
				}
			}
		}			
	}
}

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
