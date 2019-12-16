
#include "open_spiel/spiel_utils.h"


#include "open_spiel/games/twixt.h"
#include "open_spiel/games/twixt/twixtboard.h"

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
		/*long_name=*/"TwixT", GameType::Dynamics::kSequential,
		GameType::ChanceMode::kDeterministic,
		GameType::Information::kPerfectInformation, GameType::Utility::kZeroSum,
		GameType::RewardModel::kTerminal,
		/*max_num_players=*/2,
		/*min_num_players=*/2,
		/*provides_information_state=*/true,
		/*provides_information_state_as_normalized_vector=*/true,
		/*provides_observation=*/false,
		/*provides_observation_as_normalized_vector=*/false,
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

// ***************************  TwixtState *****************************************

TwixTState::~TwixTState() {
}


TwixTState::TwixTState(std::shared_ptr<const Game> game) :   State(game) {

	const TwixTGame &parent_game = static_cast<const TwixTGame&>(*game);

	mBoard = Board(
		parent_game.getBoardSize(),
		parent_game.getAnsiColorOutput()
	);

}

// ********************************   TwixTGame ************************************************

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
