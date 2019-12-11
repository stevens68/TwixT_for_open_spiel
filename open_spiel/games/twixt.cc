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

using namespace std;

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

std::string TwixTState::ActionToString(int player, Action move) const {

	return mBoard.actionToString(player, move);
}

std::string TwixTState::ToString() const {

	return mBoard.toString();
}

bool TwixTState::IsTerminal() const {

	int outlook = mBoard.getOutlook();

	return (outlook == Outlook::RED_WON ||
			outlook == Outlook::BLUE_WON ||
			outlook == Outlook::DRAW);
}

std::vector<double> TwixTState::Returns() const {

	double reward;

	if (mBoard.getOutlook() == Outlook::RED_WON) {
		reward = pow(mDiscount, mBoard.getMoveCounter());
		//reward = 1.0;
		return {reward, -reward};
	} else if (mBoard.getOutlook() == Outlook::BLUE_WON) {
		reward = pow(mDiscount, mBoard.getMoveCounter());
		//reward = 1.0;
		return {-reward, reward};
	} else {
		return {0.0, 0.0};
	}

}

std::string TwixTState::InformationState(int player) const {
	return HistoryString();
}

void TwixTState::InformationStateAsNormalizedVector(int player,
		vector<double> *values) const {

	mBoard.createNormalizedVector(mCurrentPlayer, values);

}

std::string TwixTState::Observation(int player) const {
	SpielFatalError("Observation is not implemented.");
	return "";
}

void TwixTState::ObservationAsNormalizedVector(int player,
		std::vector<double> *values) const {
	SpielFatalError("ObservationAsNormalizedVector is not implemented.");
}

std::unique_ptr<State> TwixTState::Clone() const {
	return std::unique_ptr < State > (new TwixTState(*this));
}

std::vector<Action> TwixTState::LegalActions() const {
	return mBoard.getLegalActions(mCurrentPlayer);
}


void TwixTState::DoApplyAction(Action move) {

	mBoard.applyAction(mCurrentPlayer, move);
	// toggle player
	mCurrentPlayer = 1 - mCurrentPlayer;

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
				"board_size out of range [" + to_string(kMinBoardSize) + ".."
						+ to_string(kMaxBoardSize) + "]: "
						+ to_string(mBoardSize) + "; ");
	}

	if (mDiscount <= kMinDiscount || mDiscount > kMaxDiscount) {
		SpielFatalError(
				"discount out of range [" + to_string(kMinDiscount)
						+ " < discount <= " + to_string(kMaxDiscount) + "]: "
						+ to_string(mDiscount) + "; ");
	}
}


}  // namespace twixt
}  // namespace open_spiel
