
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
		std::string ActionToString(int player, Action move) const override;

		std::string ToString() const override;
		bool IsTerminal() const override;
		std::vector<double> Returns() const override;

		std::string InformationStateString(open_spiel::Player player) const override;
		void InformationStateTensor(int, std::vector<double> *) const override;

		std::string ObservationString(int player) const override;
		void ObservationTensor (int,	std::vector<double> *) const override;

		std::unique_ptr<State> Clone() const override;

		void UndoAction(int, Action) override {};
		std::vector<Action> LegalActions() const override;

	protected:
		void DoApplyAction(Action move) override;

	private:
		int mCurrentPlayer = PLAYER_RED;         // Player zero goes first
		Board mBoard;
		double mDiscount = kDefaultDiscount;

};


class TwixTGame: public Game {

	public:
		explicit TwixTGame(const GameParameters &params);
		std::unique_ptr<State> NewInitialState() const override {
			return std::unique_ptr<State>(new TwixTState(shared_from_this()));
		}

		int NumDistinctActions() const override { return kMaxBoardSize*kMaxBoardSize; };
		int NumPlayers() const override { return PLAYER_COUNT; };
		double MinUtility() const override { return -1; };
		double UtilitySum() const override { return 0; };
		double MaxUtility() const override { return 1; };

		std::shared_ptr<const Game> Clone() const override {
			return std::shared_ptr<const Game>(new TwixTGame(*this));
		}

		std::vector<int> ObservationTensorShape() const override {
			return {};
		}

		std::vector<int> InformationStateTensorShape() const override {
			static std::vector<int> shape{ kNumPlanes, mBoardSize, (mBoardSize-2) };
			return shape;
		}

		int MaxGameLength() const { return kMaxBoardSize*kMaxBoardSize - 4 + 1; }

		bool getAnsiColorOutput() const {
			return mAnsiColorOutput;
		}

		int getBoardSize() const {
			return mBoardSize;
		}

		double getDiscount() const {
			return mDiscount;
		}

	private:
		bool mAnsiColorOutput;
		int  mBoardSize;
		double mDiscount;

};

}  // namespace twixt
}  // namespace open_spiel

#endif  // THIRD_PARTY_OPEN_SPIEL_GAMES_TWIXT_H_
