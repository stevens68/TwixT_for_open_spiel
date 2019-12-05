#include "open_spiel/games/twixt.h"

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
		  { "discount",	GameParameter(kDefaultDiscount)    },
		  { "draw_check", GameParameter(kDefaultDrawCheck) },
		  { "ansi_color_output", GameParameter(kDefaultAnsiColorOutput) }
		},
};

std::unique_ptr<Game> Factory(const GameParameters &params) {
	return std::unique_ptr < Game > (new TwixTGame(params));
}

REGISTER_SPIEL_GAME(kGameType, Factory);

}  // namespace

// ***************************  TwixtState *****************************************

TwixTState::TwixTState(std::shared_ptr<const Game> game) :
		State(game) {

	const TwixTGame &parent_game = static_cast<const TwixTGame&>(*game);

	mAnsiColorOutput = parent_game.getAnsiColorOutput();
	mBoardSize = parent_game.getBoardSize();

	mVirtualBoardSize = mBoardSize + 2 * kMargin;
	mMoveCounter = 0;

	// initialize board and mLegalActions
	InitializeBoards();
}

std::string TwixTState::ActionToString(int player, Action move) const {

	string s = "";

	s += char(int('A') + move % mBoardSize);
	s.append(to_string(mBoardSize - move / mBoardSize));
	return s;
}

std::string TwixTState::ToString() const {

	string s = "";
	Board b = mBoard;

	// head line
	s.append("     ");
	for (int y = 0; y < mBoardSize; y++) {
		string letter = "";
		letter += char(int('A') + y);
		appendColorString(&s, mAnsiColorOutput, kAnsiRed, letter + "  ");
	}
	s.append("\n");

	for (int y = mBoardSize -1 + kMargin; y >= kMargin; y--) {
		// print "before" row
		s.append("    ");
		for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
			appendBeforeRow(&b, &s, {x, y});
		}
		s.append("\n");

		// print peg row
		y - kMargin + 1 < 10 ? s.append("  ") : s.append(" ");
		appendColorString(&s, mAnsiColorOutput, kAnsiBlue,
				to_string(mBoardSize + kMargin - y) + " ");
		for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
			appendPegRow(&b, &s, {x, y});
		}
		s.append("\n");

		// print "after" row
		s.append("    ");
		for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
			appendAfterRow(&b, &s, {x, y});
		}
		s.append("\n");
	}
	s.append("\n");

	if (mSwapped)
		s.append("[swapped]");

	switch (mOutlook) {
	case Outlook::BLUE_WONTWIN:
		s.append("[O won't win]");
		break;
	case Outlook::RED_WONTWIN:
		s.append("[X won't win]");
		break;
	case Outlook::RED_WON:
		s.append("[X has won]");
		break;
	case Outlook::BLUE_WON:
		s.append("[O has won]");
		break;
	case Outlook::DRAW:
		s.append("[draw]");
	default:
		break;
	}

	s.append("\n");
	for (int color = 0; color < 2; color++) {
		vector<Action>::iterator it;
		vector<Action> v = mLegalActions[color];
		for (it = v.begin(); it != v.end(); it++) {
			s.append(to_string(*it));
			s.append(" ");
		}
		s.append("\n");
	}

	return s;
}

bool TwixTState::IsTerminal() const {

	return (mOutlook == Outlook::RED_WON || mOutlook == Outlook::BLUE_WON
			|| mOutlook == Outlook::DRAW);
}

std::vector<double> TwixTState::Returns() const {

	double reward;

	if (mOutlook == Outlook::RED_WON) {
		reward = pow(mDiscount, mMoveCounter);
		//reward = 1.0;
		return {reward, -reward};
	} else if (mOutlook == Outlook::BLUE_WON) {
		reward = pow(mDiscount, mMoveCounter);
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

	values->resize(0);
	values->reserve(kNumPlanes * mBoardSize * (mBoardSize - 2));

	for (int color = 0; color < kNumPlayers; color++) {
		// add peg configuration
		AddPegPlane(color, values);

		// add link configuration
		for (int dir = 0; dir < (kNumLinkDirections / 2); dir++) {
			AddLinkPlane(color, dir, values);
		}
	}

	// add plane that indicates who's turn it is
	AddBinaryPlane(values);

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
	return mLegalActions[mCurrentPlayer];
}


void TwixTState::SetSurroundingLinks(Coordinates c) {

	Board *b = &mBoard;
	PegSet *startSet = &(b->linkedToStart[mCurrentPlayer]);
	PegSet *endSet = &(b->linkedToEnd[mCurrentPlayer]);

	bool linkedToNeutral = false;
	bool linkedToStart = false;
	bool linkedToEnd = false;

	for (int dir = 0; dir < kNumLinkDirections; dir++) {
		LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
		Coordinates cd = ld->offsets;

		if (b->cell[c.first + cd.first][c.second + cd.second].peg == b->cell[c.first][c.second].peg
				&& !(b->cell[c.first][c.second].blockedLinks & (1UL << dir))) {
			// target peg has same color and link is not blocked
			// set link from/to this peg
			b->cell[c.first][c.second].links |= (1UL << dir);
			b->cell[c.first + cd.first][c.second + cd.second].links |= 1UL << getOppDir(dir);

			// set blockers and check if we have
			// crossed the current drawPath of red or blue
			setBlockers(b, c, cd, ld, true);

			// check if peg we link to is in a pegset (START / END)
			if (isInPegSet(startSet, c + cd)) {
				addToPegSet(startSet, c);
				linkedToStart = true;
			}
			if (isInPegSet(endSet, c + cd)) {
				addToPegSet(endSet, c);
				linkedToEnd = true;
			}
			if (!isInPegSet(startSet, c + cd)
					&& !isInPegSet(endSet, c + cd)) {
				linkedToNeutral = true;
			}

		}
	}

	//check if we need to explore further
	if (isInPegSet(startSet, c) && linkedToNeutral) {

		// case: new peg is in PegSet START and linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet START
		ExploreLocalGraph(b, c, startSet);
	}
	if (isInPegSet(endSet, c) && linkedToNeutral) {
		// case: new peg is in PegSet END and is linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet END
		ExploreLocalGraph(b, c, endSet);
	}

}

void removeLegalAction(vector<Action> *la, Action move) {
	// remove from unsorted vector
	vector<Action>::iterator it;
	 it = find(la->begin(), la->end(), move);
	 if (it != la->end()) la->erase(it);

	 // remove from sorted vector
	 /*
	 vector<Action>::iterator it = std::lower_bound(la->begin(), la->end(), move);
	 if (it != la->end() && *it == move) {
		 la->erase(it);
	 }
	 */
}

void TwixTState::DoApplyAction(Action move) {

	vector<Action> *la;	// legal actions
	Board *b = &mBoard;

	/* a sample game for debug purposes */
	/*
	 if (mMoveCounter == 0)
	 move = 8;  //C1   red
	 if (mMoveCounter == 1)
	 move = 27;  //D5   blue
	 if (mMoveCounter == 2)
	 move = 16;  //E3-  red
	 if (mMoveCounter == 3)
	 move = 14;  //C3-  blue
	 if (mMoveCounter == 4)
	 move = 28;  //E5-  red
	 if (mMoveCounter == 5)
	 move = 6;  //A2-  blue D3 B3
	 if (mMoveCounter == 6)
	 move = 15;  //D3-  red  ...  blue won't win
	 if (mMoveCounter == 7)
	 move = 13;  //B3   blue
	 if (mMoveCounter == 8)
	 move = 25;  //B5   red
	 if (mMoveCounter == 9)
	 move = 7;  //B2   blue
	 if (mMoveCounter == 10)
	 move = 32;  //C6-  red  .... draw!
	 if (mMoveCounter == 11)
	 move = 21;  //D4
	 */

	if (mMoveCounter == 1) {
		if (move == mMoveOne) {
			// player 2 swapped
			mSwapped = true;
			// turn move 90° clockwise
			move = (move % mBoardSize) * mBoardSize
					+ (mBoardSize - (move / mBoardSize) - 1);

			// undo first move (peg and legal actions)
			InitializeBoards();
		} else {
			// not swapped: regular move
			// remove move #1 from legal moves
			removeLegalAction(&mLegalActions[RED], mMoveOne);
			removeLegalAction(&mLegalActions[BLUE], mMoveOne);
		}
	}

	// calculate virtual coords from move
	Coordinates c = { (int) move % mBoardSize + kMargin, (int) move / mBoardSize + kMargin };

	// set peg on regular board
	b->cell[c.first][c.second].peg = mCurrentPlayer;

	// if peg is on borderline, add it to respective peg list
	int compare = mCurrentPlayer == RED ? c.second : c.first;
	if (compare - kMargin == 0) {
		addToPegSet(&(b->linkedToStart[mCurrentPlayer]), c);
	} else if (compare - kMargin == mBoardSize - 1) {
		addToPegSet(&(b->linkedToEnd[mCurrentPlayer]), c);
	}

	if (mDrawCheck) {
		// check if peg is in opponent's drawPath...
		if (isPegInDrawPath(&(mBoard.drawPath[1 - mCurrentPlayer]), c, mBoardSize)) {
			// ...and clear it
			mBoard.drawPath[1 - mCurrentPlayer].clear();
		}
	}

	// set links
	SetSurroundingLinks(c);

	if (mDrawCheck) {
		vector<int> drawPath;
		drawPath.reserve(b->boardSize * b->boardSize);

		for (int color = RED; color <= BLUE; color++) {
			// check if we need to repair drawPaths
			if (mBoard.canWin[color] && mBoard.drawPath[color].size() == 0) {
				// we have to find a new drawPath
				//cerr << to_string(color) << ": looking for new draw path: " << endl;
				//cerr << ToString() << endl;

				initializeDrawPath(&mBoard, color);
			}
		}
	}

	mMoveCounter++;

	if (mMoveCounter == 1) {
		// do not remove the move from legal actions but store it
		// because player 2 might want to swap
		mMoveOne = move;
	} else {
		// remove move one from mLegalActions
		removeLegalAction(&mLegalActions[RED], move);
		removeLegalAction(&mLegalActions[BLUE], move);
	}

	// Update the predicted result and update mCurrentPlayer...
	UpdateOutlook(c);

	// toggle current player
	mCurrentPlayer = 1 - mCurrentPlayer;

}

void TwixTState::InitializeBoards() {

	mBoard.ansiColorOutput = mAnsiColorOutput;
	mBoard.boardSize = mBoardSize;

	initializePegs(&mBoard);

	if (mDrawCheck)  {
		initializeDrawPath(&mBoard, RED);
		initializeDrawPath(&mBoard, BLUE);
	}

	initializeLegalActions(&mBoard, RED, &(mLegalActions[RED]));
	initializeLegalActions(&mBoard, BLUE, &(mLegalActions[BLUE]));

}

void TwixTState::UpdateOutlook(Coordinates c) {

	// check for WIN
	bool connectedToStart = isInPegSet(&(mBoard.linkedToStart[mCurrentPlayer]), c);
	bool connectedToEnd = isInPegSet(&(mBoard.linkedToEnd[mCurrentPlayer]), c);
	if (connectedToStart && connectedToEnd) {
		// peg is linked to both boarder lines
		mOutlook = mCurrentPlayer == RED ? Outlook::RED_WON : Outlook::BLUE_WON;
		return;
	}

	// check if we are early in the game...
	if (mMoveCounter < mBoardSize - 1) {
		// less than 5 moves played on a 6x6 board => no win or draw possible, no need to update
		return;
	}

	// check for draw
	if (mDrawCheck) {
		if (!mBoard.canWin[RED]) {
			mOutlook = Outlook::RED_WONTWIN;
			if (!mBoard.canWin[BLUE]) {
				mOutlook = Outlook::DRAW;
				//cerr << "draw! " << to_string(100.0 * mMoveCounter / (mBoardSize*mBoardSize-4)) << endl;
				return;
			}
		} else if (!mBoard.canWin[BLUE]) {
			mOutlook = Outlook::BLUE_WONTWIN;
		}
	}

	//check if opponent (player to turn next) has any legal moves left
	if (mLegalActions[1 - mCurrentPlayer].size() == 0) {
		mOutlook = Outlook::DRAW;
		return;
	}

}

void TwixTState::ExploreLocalGraph(Board *b, Coordinates c, PegSet *pegSet) {

	unsigned int dir;
	int idx;
	// check all linked neighbors
	for (dir = 1, idx = 0; dir <= b->cell[c.first][c.second].links; dir <<= 1, idx++) {
		if (b->cell[c.first][c.second].links & dir) {
			LinkDescriptor *ld = &(kLinkDescriptorTable[idx]);
			Coordinates cd = ld->offsets;
			if (!isInPegSet(pegSet, c + cd)) {
				// linked neighbor is NOT yet member of PegSet => add it and explore
				addToPegSet(pegSet, c + cd);
				ExploreLocalGraph(b, c + cd, pegSet);
			}
		}
	}
}

// Adds a binary plane to the information state vector
// all ones or all zeros
void TwixTState::AddBinaryPlane(std::vector<double> *values) const {
	double normalizedVal = static_cast<double>(mCurrentPlayer);
	values->insert(values->end(), mBoardSize * (mBoardSize - 2), normalizedVal);
}

// Adds a plane to the information state vector corresponding to the presence
// and absence of the given link & color &direction at each square.
void TwixTState::AddLinkPlane(int color, int dir,
		std::vector<double> *values) const {

	if (color == RED) {
		for (int y = 0 + kMargin; y < mBoardSize + kMargin; y++) {
			for (int x = 1 + kMargin; x < mBoardSize - 1 + kMargin; x++) {
				if (mBoard.cell[x][y].peg == color
						&& mBoard.cell[x][y].links & (1UL << dir)) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	} else {
		/* BLUE => turn board 90° clockwise */
		for (int y = mBoardSize - 2 + kMargin; y > 0 + kMargin; y--) {
			for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
				if (mBoard.cell[x][y].peg == color
						&& mBoard.cell[x][y].links & (1UL << dir)) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	}
}

// Adds a plane to the information state vector
// we only look at pegs without links
void TwixTState::AddPegPlane(int color, std::vector<double> *values) const {

	if (color == RED) {
		for (int y = 0 + kMargin; y < mBoardSize + kMargin; y++) {
			for (int x = 1 + kMargin; x < mBoardSize - 1 + kMargin; x++) {
				if (mBoard.cell[x][y].peg == color
						&& mBoard.cell[x][y].links == 0) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	} else {
		/* turn board" 90° clockwise */
		for (int y = mBoardSize - 2 + kMargin; y > 0 + kMargin; y--) {
			for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
				if (mBoard.cell[x][y].peg == color
						&& mBoard.cell[x][y].links == 0) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	}

}

// ********************************   TwixTGame ************************************************

TwixTGame::TwixTGame(const GameParameters &params) :
		Game(kGameType, params),
		mAnsiColorOutput(
				ParameterValue<bool>("ansi_color_output",kDefaultAnsiColorOutput)
		),
		mDrawCheck(
				ParameterValue<bool>("draw_check",kDefaultDrawCheck)
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

	if (mDiscount <= 0.0 || mDiscount > 1.0) {
		SpielFatalError(
				"discount out of range [" + to_string(kMinDiscount)
						+ " < discount <= " + to_string(kMaxDiscount) + "]: "
						+ to_string(mDiscount) + "; ");
	}

	mMaxGameLength = mBoardSize * mBoardSize - 4 + 1;
}

/*
 int TwixTGame::NumDistinctActions() const {
 return mBoardSize * mBoardSize;
 }

 int TwixTGame::NumPlayers() const {
 return kNumPlayers;
 }

 double TwixTGame::MinUtility() const {
 return -1;
 }

 double TwixTGame::UtilitySum() const {
 return 0;
 }

 double TwixTGame::MaxUtility() const {
 return 1;
 }

 std::vector<int> TwixTGame::ObservationNormalizedVectorShape() const {
 return {};
 }


 int TwixTGame::MaxGameLength() const {
 // all pegs set (except the corners plus swapped, which counts as a move
 return mMaxGameLength;
 }
 */

// *********************************************************************

// helper
bool isLinkInDrawPath(Board *b, Coordinates c, int dir, int color) {

	vector<int> *wp = &(b->drawPath[color]);

	// if drawPath has size 0, we don't have a drawPath anymore
	// and we don't have to check again
	if (wp->size() == 0)
		false;

	int linkCode = getLinkCode(c, dir, b->boardSize);
	// check if this edge is part of the drawPath
	vector<int>::iterator it;
	it = find(wp->begin(), wp->end(), linkCode);
	return (it != wp->end());
}

bool isPegInDrawPath(std::vector<int> *wp, Coordinates c, int boardSize) {

	// check if this peg is part of the drawPath
	vector<int>::iterator it;
	it = find(wp->begin(), wp->end(), getAction(c, boardSize));
	return (it != wp->end());
}


void initializePegs(Board *b) {

	int virtualBoardSize = b->boardSize + 2 * kMargin;

	//adjust dim 1 of board
	b->cell.resize(virtualBoardSize);

	//  initialize peg sets to false (2 sets per player)
	for (int ii = 0; ii < kMaxVirtualBoardSize * kMaxVirtualBoardSize; ii++) {
		b->linkedToStart[RED][ii] = false;
		b->linkedToEnd[RED][ii] = false;

		b->linkedToStart[BLUE][ii] = false;
		b->linkedToEnd[BLUE][ii] = false;
	}

	// initialize board with pegs (empty or overboard)
	for (int x = 0; x < virtualBoardSize; x++) {
		//adjust dim 2 of board
		b->cell[x].resize(virtualBoardSize);

		for (int y = 0; y < virtualBoardSize; y++) {
			// links
			b->cell[x][y].links = 0;
			b->cell[x][y].blockedLinks = 0;
			// set pegs to EMPTY, RED or BLUE depending on boardType and position
			if (isOnMargin({x, y}, b->boardSize)) {
				b->cell[x][y].peg = OVERBOARD;
			} else { // regular board
				b->cell[x][y].peg = EMPTY;
			} // if
		} // for x
	} // for y

}

void initializeLegalActions(Board *b, int color, vector<Action> *la) {

	la->clear();
	la->reserve(b->boardSize * b->boardSize);

	int virtualBoardSize = b->boardSize + 2 * kMargin;

	for (int y = 0; y < virtualBoardSize; y++) {
		for (int x = 0; x < virtualBoardSize; x++) {
			if (!isOnBorderline({x, y}, 1 - color, b->boardSize)
					&& !isOnMargin({x, y}, b->boardSize)) {
				int action = (y-kMargin) * b->boardSize + (x-kMargin);
				la->push_back(action);
			}
		} // for y
	} // for x
}

void initializeDrawPath(Board *b, int color) {

	b->canWin[color] = false;
	b->drawPath[color].clear();

	Board copyBoard = *b;

	for (int ii = 1; ii < b->boardSize - 1; ii++) {
		//cerr << to_string(ii) << endl;
		int x = color == RED ? ii + kMargin : 0 + kMargin;
		int y = color == RED ? 0 + kMargin : ii + kMargin;
		copyBoard.drawPath[color].clear();

		// check surrounfing links
		if (! b->cell[x][y].deadPeg[color]) {
			if (findDrawPath(b, {x, y}, -1, color, copyBoard)) {
				// >>debug
				//printDrawPath(&(b->drawPath[color]), copyBoard.boardSize, "+++: ");
				// <<debug
				b->canWin[color] = true;
				return;
			}
		}
	}

	//cerr << "NOT FOUND" << endl;

}

int getAction(Coordinates c, int boardSize) {
	return (c.second - kMargin) * boardSize + (c.first - kMargin);
}

bool findDrawPath(Board *b, Coordinates c, int dir, int color, Board pb) {

	Coordinates cn;

	if (dir >= 0) {
		// => set source peg & append source peg code
		pb.cell[c.first][c.second].peg = color;
		pb.drawPath[color].push_back(getAction(c, pb.boardSize));
		// set link that starts from this peg (found before)
		Coordinates cd = kLinkDescriptorTable[dir].offsets;

		// => set link (both perspectives) and blockers
		pb.cell[c.first][c.second].links |= (1UL << dir);
		pb.cell[c.first + cd.first][c.second + cd.second].links |= (1UL << getOppDir(dir));
		setBlockers(&pb, c, cd, &(kLinkDescriptorTable[dir]), false);

		// => append link code
		int linkCode;
		if (dir < kNumLinkDirections / 2) {
			linkCode = getLinkCode(c, dir, pb.boardSize);
		} else {
			linkCode = getLinkCode(c + cd, getOppDir(dir),
					pb.boardSize);
		}
		pb.drawPath[color].push_back(linkCode);

		// check if we reached the opposite borderline
		int compare = color == RED ? c.second+cd.second : c.first+cd.first;
		if (compare == pb.boardSize - 1 + kMargin) {
			// we reached the opposite borderline line, save drawPath
			pb.cell[c.first+cd.first][c.second+cd.second].peg = color;
			pb.drawPath[color].push_back(getAction(c + cd, pb.boardSize));
			// copy final drawPath
			b->drawPath[color] = pb.drawPath[color];
			return true;
		}
		cn = c + cd;
	} else {
		cn = c;
	}

	//printDrawPath((&pb.drawPath[color]), pb.boardSize, "           >:");

	// search deeper..
	// check surrounding link options...
	for (int i = 0; i < kNumLinkDirections; i++) {
		int nDir = LINKORDER[color][i];
		LinkDescriptor *ld = &(kLinkDescriptorTable[nDir]);
		Coordinates cnd = ld->offsets;

		if ((!isPegInDrawPath(&(pb.drawPath[color]), cn + cnd,
				pb.boardSize))
				&& (pb.cell[cn.first + cnd.first][cn.second + cnd.second].peg == color
						|| pb.cell[cn.first + cnd.first][cn.second + cnd.second].peg == EMPTY)
				&& !isOnBorderline(cn + cnd, 1 - color, pb.boardSize)
				&& !(pb.cell[cn.first][cn.second].blockedLinks & (1UL << nDir))
				&& ! b->cell[cn.first][cn.second].deadPeg[color]) {
			// dir doesn't point to a peg we already visited during this exploration (loop)
			// AND (target field is empty OR peg has same color)
			// AND target field is not on opponent's borderline
			// AND link is not blocked
			// AND target peg is no dead end that we detected previously

			if (findDrawPath(b, cn, nDir, color, pb)) {
				return true;
			}
		}
	}
	// there is no draw path including this peg
	b->cell[cn.first][cn.second].deadPeg[color] = true;

	return false;
}



std::string coordsToString(Coordinates c, int boardSize) {
	string s;
	s = char(int('A') + (c.first - kMargin)) + to_string(boardSize + kMargin - c.second);
	return s;
}

bool isOnBorderline(Coordinates c, int color, int boardSize) {

	if (color == RED) {
		return ((c.second == kMargin || c.second == kMargin + boardSize - 1)
				&& (c.first > kMargin && c.first < kMargin + boardSize - 1));
	} else {
		return ((c.first == kMargin || c.first == kMargin + boardSize - 1)
				&& (c.second > kMargin && c.second < kMargin + boardSize - 1));
	}
}

bool isOnMargin(Coordinates c, int boardSize) {

	return (c.second < kMargin || c.second > kMargin + boardSize - 1 ||
			c.first < kMargin || c.first > kMargin + boardSize - 1 ||
			// corner case
			((c.first == kMargin || c.first == kMargin + boardSize - 1) &&
			 (c.second == kMargin || c.second == kMargin + boardSize - 1)));
}

bool isInPegSet(PegSet *p, Coordinates c) {
	return (*p)[c.second * kMaxVirtualBoardSize + c.first];
}

void addToPegSet(PegSet *p, Coordinates c) {
	(*p)[c.second * kMaxVirtualBoardSize + c.first] = true;
}

void appendBeforeRow(Board *b, string *s, Coordinates c) {

	// -1, +1
	int len = s->length();
	appendLinkChar(b, s, getNeighbour(c, {-1, 0}), LinkDirections::ENE, "/");
	appendLinkChar(b, s, getNeighbour(c, {-1,-1}), LinkDirections::NNE, "/");
	appendLinkChar(b, s, getNeighbour(c, { 0, 0}), LinkDirections::WNW, "_");
	if (len == s->length())
		s->append(" ");

	//  0, +1
	len = s->length();
	appendLinkChar(b, s, c, LinkDirections::NNE, "|");
	if (len == s->length())
		appendLinkChar(b, s, c, LinkDirections::NNW, "|");
	if (len == s->length())
		s->append(" ");

	// +1, +1
	len = s->length();
	appendLinkChar(b, s, getNeighbour(c, {+1, 0}), LinkDirections::WNW, "\\");
	appendLinkChar(b, s, getNeighbour(c, {+1,-1}), LinkDirections::NNW, "\\");
	appendLinkChar(b, s, getNeighbour(c, { 0, 0}), LinkDirections::ENE, "_");
	if (len == s->length())
		s->append(" ");

}

void appendPegRow(Board *b, string *s, Coordinates c) {

	// -1, 0
	int len = s->length();
	appendLinkChar(b, s, getNeighbour(c, {-1,-1}), LinkDirections::NNE, "|");
	appendLinkChar(b, s, getNeighbour(c, { 0, 0}), LinkDirections::WSW, "_");
	if (len == s->length())
		s->append(" ");

	//  0,  0
	appendPegChar(b, s, c);

	// +1, 0
	len = s->length();
	appendLinkChar(b, s, c + (Coordinates) {+1,-1}, LinkDirections::NNW, "|");
	appendLinkChar(b, s, c + (Coordinates) { 0, 0}, LinkDirections::ESE, "_");
	if (len == s->length())
		s->append(" ");

}

void appendAfterRow(Board *b, string *s, Coordinates c) {

	// -1, -1
	int len = s->length();
	appendLinkChar(b, s, getNeighbour(c, {+1, -1}), LinkDirections::WNW, "\\");
	appendLinkChar(b, s, getNeighbour(c, { 0, -1}), LinkDirections::NNW, "\\");
	if (len == s->length())
		s->append(" ");

	//  0, -1
	len = s->length();
	appendLinkChar(b, s, getNeighbour(c, {-1, -1}), LinkDirections::ENE, "_");
	appendLinkChar(b, s, getNeighbour(c, {+1, -1}), LinkDirections::WNW, "_");
	appendLinkChar(b, s, c, LinkDirections::SSW, "|");
	if (len == s->length()) {
		appendLinkChar(b, s, c, LinkDirections::SSE, "|");
	}
	if (len == s->length())
		s->append(" ");

	// -1, -1
	len = s->length();
	appendLinkChar(b, s, getNeighbour(c, {-1, -1}), LinkDirections::ENE, "/");
	appendLinkChar(b, s, getNeighbour(c, { 0, -1}), LinkDirections::NNE, "/");
	if (len == s->length())
		s->append(" ");
}

void appendLinkChar(Board *b, string *s, Coordinates c, enum LinkDirections dir,
		string linkChar) {
	if (b->cell[c.first][c.second].links & (1UL << dir)) {
		if (b->cell[c.first][c.second].peg == RED) {
			appendColorString(s, b->ansiColorOutput, kAnsiRed, linkChar);
		} else if (b->cell[c.first][c.second].peg == BLUE) {
			appendColorString(s, b->ansiColorOutput, kAnsiBlue, linkChar);
		} else {
			s->append(linkChar);
		}
	}
}

void appendColorString(string *s, bool ansiColorOutput, string colorString,
		string appString) {
	s->append(ansiColorOutput ? colorString : ""); // make it colored
	s->append(appString);
	s->append(ansiColorOutput ? kAnsiDefault : ""); // make it default
}

void appendPegChar(Board *b, string *s, Coordinates c) {
	if (b->cell[c.first][c.second].peg == RED) {
		// X
		appendColorString(s, b->ansiColorOutput, kAnsiRed, "X");
	} else if (b->cell[c.first][c.second].peg == BLUE) {
		// O
		appendColorString(s, b->ansiColorOutput, kAnsiBlue, "O");
	} else if (isOnMargin(c, b->boardSize)) {
		// corner
		s->append(" ");
	} else if (c.first - kMargin == 0 || c.first - kMargin == b->boardSize - 1) {
		// empty . (blue border line)
		appendColorString(s, b->ansiColorOutput, kAnsiBlue, ".");
	} else if (c.second - kMargin == 0 || c.second - kMargin == b->boardSize - 1) {
		// empty . (red border line)
		appendColorString(s, b->ansiColorOutput, kAnsiRed, ".");
	} else {
		// empty (non border line)
		s->append(".");
	}
}

int getOppDir(int dir) {
	return (dir + kNumLinkDirections / 2) % kNumLinkDirections;
}

int getLinkCode(Coordinates c, int dir, int boardSize) {
	return -  (10 * ((c.second-kMargin) * boardSize + (c.first-kMargin)) + dir);
}

string linkCodeToString(int linkCode, int boardSize) {

	int dir = -linkCode % 10;
	Coordinates c = { (-linkCode/10) % boardSize + kMargin, (-linkCode/10) / boardSize + kMargin };

	LinkDescriptor *ld = &kLinkDescriptorTable[dir];
	Coordinates cd = kLinkDescriptorTable[dir].offsets;

	return coordsToString(c, boardSize) + "-" + coordsToString(c + cd, boardSize);

}

void setBlockers(Board *b, Coordinates c, Coordinates cd, LinkDescriptor *ld,
		bool checkDrawPathCrossing) {

	for (auto &&entry : ld->blockingLinks) {
		// get offsets for eastern direction
		Coordinates cd = entry.first;
		int blockingDir = entry.second;
		int bitmap = 1UL << blockingDir;
		// block this link (eastern)
		b->cell[c.first + cd.first][c.second + cd.second].blockedLinks |= bitmap;

		// get offsets for western direction
		Coordinates cdd = kLinkDescriptorTable[blockingDir].offsets;
		bitmap = 1UL << getOppDir(blockingDir);
		// block this link (western)
		b->cell[c.first + cd.first + cdd.first][c.second + cd.second + cdd.second].blockedLinks |= bitmap;

		//check if link (eastern) is in drawPath
		if (checkDrawPathCrossing) {
			if (isLinkInDrawPath(b, c + cd, blockingDir, RED)) {
				// the new link crosses red's drawPath
				b->drawPath[RED].clear();
			}
			if (isLinkInDrawPath(b, c + cd, blockingDir, BLUE)) {
				// the new link crosses red's drawPath
				b->drawPath[BLUE].clear();
			}
		}
	}

}

void printDrawPath(vector<int> *dp, int boardSize, string prefix) {

	cerr << prefix;
	vector<int>::iterator it;
	for (it = dp->begin(); it != dp->end(); it++) {
		if (*it > 0) {
			string s = "";
			s += char(int('A') + *it % boardSize);
			s.append(to_string(*it / boardSize + 1));
			cerr << s << ",";
		}
	}
	cerr << endl;
}

Coordinates getNeighbour(Coordinates c, Coordinates offset) {
	return c + offset;
}

}  // namespace twixt
}  // namespace open_spiel

