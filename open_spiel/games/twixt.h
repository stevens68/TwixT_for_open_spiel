
#ifndef THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXT_H_
#define THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXT_H_

#include "open_spiel/spiel.h"
#include "open_spiel/games/twixt/twixtboard.h"

// https://en.wikipedia.org/wiki/TwixT

namespace open_spiel {
namespace twixt {


class TwixTState: public State {
	public:
		~TwixTState();
		TwixTState(std::shared_ptr<const Game> game);

		TwixTState(const TwixTState&) = default;
		TwixTState& operator=(const TwixTState&) = default;

		int CurrentPlayer() const override { return mCurrentPlayer; };
		std::string ActionToString(int player, Action move) const override {
			return mBoard.actionToString(player, move);;
		}

		std::string ToString() const override { return mBoard.toString(); };
		bool IsTerminal() const override {
			int result = mBoard.getResult();
			return (result == Result::RED_WON || result == Result::BLUE_WON || result == Result::DRAW);
		};

		std::vector<double> Returns() const override {
			double reward;
			if (mBoard.getResult() == Result::OPEN || mBoard.getResult() == Result::DRAW) { return {0.0, 0.0}; }
			else {
				reward = pow(mDiscount, mBoard.getMoveCounter());
				if (mBoard.getResult() == Result::RED_WON) { return {reward, -reward}; }
				else { return {-reward, reward}; }
			}
		};

		std::string InformationStateString(open_spiel::Player player) const override { return HistoryString(); };
		void InformationStateTensor(int player, std::vector<double> *values) const override {
			mBoard.createNormalizedVector(player, values);
		};

		std::string ObservationString(int player) const override {
			SpielFatalError("ObservationString is not implemented.");
			return "";
		};

		void ObservationTensor (int player,	std::vector<double> *values) const override {
			SpielFatalError("ObservationTensor is not implemented.");
		};

		std::unique_ptr<State> Clone() const override {
			return std::unique_ptr < State > (new TwixTState(*this));
		};

		void UndoAction(int, Action) override {};

		std::vector<Action> LegalActions() const override {
			return mBoard.getLegalActions(mCurrentPlayer);
		};

	protected:
		void DoApplyAction(Action move) override {
			mBoard.applyAction(mCurrentPlayer, move);
			if (mBoard.getResult() == Result::OPEN) { mCurrentPlayer = 1 - mCurrentPlayer; }
			else { mCurrentPlayer = kTerminalPlayerId; }
		};

	private:
		int mCurrentPlayer = PLAYER_RED;         // Player zero goes first
		Board mBoard;
		double mDiscount = kDefaultDiscount;

};


class TwixTGame: public Game {

	public:
		explicit TwixTGame(const GameParameters &params);
		inline std::unique_ptr<State> NewInitialState() const override {
			return std::unique_ptr<State>(new TwixTState(shared_from_this()));
		}

		inline int NumDistinctActions() const override { return kMaxBoardSize*kMaxBoardSize; };
		inline int NumPlayers() const override { return PLAYER_COUNT; };
		inline double MinUtility() const override { return -1; };
		inline double UtilitySum() const override { return 0; };
		inline double MaxUtility() const override { return 1; };

		inline std::shared_ptr<const Game> Clone() const override {
			return std::shared_ptr<const Game>(new TwixTGame(*this));
		}

		inline std::vector<int> ObservationTensorShape() const override {
			return {};
		}

		inline std::vector<int> InformationStateTensorShape() const override {
			static std::vector<int> shape{ kNumPlanes, mBoardSize, (mBoardSize-2) };
			return shape;
		}

		inline int MaxGameLength() const { return kMaxBoardSize*kMaxBoardSize - 4 + 1; }
		inline bool getAnsiColorOutput() const { return mAnsiColorOutput; }
		inline int getBoardSize() const { return mBoardSize; }
		inline double getDiscount() const { return mDiscount; }

	private:
		bool mAnsiColorOutput;
		int  mBoardSize;
		double mDiscount;

};

}  // namespace twixt
}  // namespace open_spiel

#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXT_H_
