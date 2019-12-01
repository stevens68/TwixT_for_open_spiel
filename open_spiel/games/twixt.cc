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
	s.append(to_string(move / mBoardSize + 1));
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

	for (int y = 0 + kMargin; y < mBoardSize + kMargin; y++) {
		// print "before" row
		s.append("    ");
		for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
			appendBeforeRow(&b, &s, x, y);
		}
		s.append("\n");

		// print peg row
		y - kMargin + 1 < 10 ? s.append("  ") : s.append(" ");
		appendColorString(&s, mAnsiColorOutput, kAnsiBlue,
				to_string(y - kMargin + 1) + " ");
		for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
			appendPegRow(&b, &s, x, y);
		}
		s.append("\n");

		// print "after" row
		s.append("    ");
		for (int x = 0 + kMargin; x < mBoardSize + kMargin; x++) {
			appendAfterRow(&b, &s, x, y);
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

	/*
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
	*/

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

void TwixTState::SetSurroundingLinks(int x, int y) {

	Board *b = &mBoard;
	PegSet *startSet = &(b->linkedToStart[mCurrentPlayer]);
	PegSet *endSet = &(b->linkedToEnd[mCurrentPlayer]);

	bool linkedToNeutral = false;
	bool linkedToStart = false;
	bool linkedToEnd = false;

	for (int dir = 0; dir < kNumLinkDirections; dir++) {
		LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
		int dx = ld->offsets.first;
		int dy = ld->offsets.second;

		if (b->cell[x + dx][y + dy].peg == b->cell[x][y].peg
				&& !(b->cell[x][y].blockedLinks & (1UL << dir))) {
			// target peg has same color and link is not blocked
			// set link from/to this peg
			b->cell[x][y].links |= (1UL << dir);
			b->cell[x + dx][y + dy].links |= 1UL << getOppDir(dir);

			// set blockers and check if we have
			// crossed the current drawPath of red or blue
			setBlockers(b, x, y, dx, dy, ld, true);

			// check if peg we link to is in a pegset (START / END)
			if (isInPegSet(startSet, x + dx, y + dy)) {
				addToPegSet(startSet, x, y);
				linkedToStart = true;
			}
			if (isInPegSet(endSet, x + dx, y + dy)) {
				addToPegSet(endSet, x, y);
				linkedToEnd = true;
			}
			if (!isInPegSet(startSet, x + dx, y + dy)
					&& !isInPegSet(endSet, x + dx, y + dy)) {
				linkedToNeutral = true;
			}

		}
	}

	//check if we need to explore further
	if (isInPegSet(startSet, x, y) && linkedToNeutral) {

		// case: new peg is in PegSet START and linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet START
		ExploreLocalGraph(b, x, y, startSet);
	}
	if (isInPegSet(endSet, x, y) && linkedToNeutral) {
		// case: new peg is in PegSet END and is linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet END
		ExploreLocalGraph(b, x, y, endSet);
	}

}

bool isLinkInDrawPath(Board *b, int x, int y, int dir, int color) {

	vector<int> *wp = &(b->drawPath[color]);

	// if drawPath has size 0, we don't have a drawPath anymore
	// and we don't have to check again
	if (wp->size() == 0)
		false;

	int linkCode = getLinkCode(x, y, dir, b->boardSize);
	// check if this edge is part of the drawPath
	vector<int>::iterator it;
	it = find(wp->begin(), wp->end(), linkCode);
	return (it != wp->end());
}

bool isPegInDrawPath(std::vector<int> *wp, int x, int y, int boardSize) {

	// check if this peg is part of the drawPath
	vector<int>::iterator it;
	it = find(wp->begin(), wp->end(), getAction(x, y, boardSize));
	return (it != wp->end());
}

void TwixTState::DoApplyAction(Action move) {

	int x;
	int y;
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
	y = move / mBoardSize + kMargin;
	x = move % mBoardSize + kMargin;

	// set peg on regular board
	b->cell[x][y].peg = mCurrentPlayer;

	// if peg is on borderline, add it to respective peg list
	int compare = mCurrentPlayer == RED ? y : x;
	if (compare - kMargin == 0) {
		addToPegSet(&(b->linkedToStart[mCurrentPlayer]), x, y);
	} else if (compare - kMargin == mBoardSize - 1) {
		addToPegSet(&(b->linkedToEnd[mCurrentPlayer]), x, y);
	}

	if (mDrawCheck) {
		// check if peg is in opponent's drawPath...
		if (isPegInDrawPath(&(mBoard.drawPath[1 - mCurrentPlayer]), x, y,
				mBoardSize)) {
			// ...and clear it
			mBoard.drawPath[1 - mCurrentPlayer].clear();
		}
	}

	// set links
	SetSurroundingLinks(x, y);

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
	UpdateOutlook(x, y);

	// toggle current player
	mCurrentPlayer = 1 - mCurrentPlayer;

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
			if (isOnMargin(x, y, b->boardSize, true)) {
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
			int action = y * b->boardSize + x;
			if (!isOnBorderline(x, y, 1 - color, b->boardSize, false)
					&& !isOnMargin(x, y, b->boardSize, false)) {
				// legal action for <color> only
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
			if (findDrawPath(b, x, y, -1, color, copyBoard)) {
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

int getAction(int x, int y, int boardSize) {
	return (y - kMargin) * boardSize + (x - kMargin);
}

bool findDrawPath(Board *b, int x, int y, int dir, int color, Board pb) {

	int nx;
	int ny;

	if (dir >= 0) {
		// => set source peg & append source peg code
		pb.cell[x][y].peg = color;
		pb.drawPath[color].push_back(getAction(x, y, pb.boardSize));
		// set link that starts from this peg (found before)
		int dx = kLinkDescriptorTable[dir].offsets.first;
		int dy = kLinkDescriptorTable[dir].offsets.second;

		// => set link (both perspectives) and blockers
		pb.cell[x][y].links |= (1UL << dir);
		pb.cell[x + dx][y + dy].links |= (1UL << getOppDir(dir));
		setBlockers(&pb, x, y, dx, dy, &(kLinkDescriptorTable[dir]), false);

		// => append link code
		int linkCode;
		if (dir < kNumLinkDirections / 2) {
			linkCode = getLinkCode(x, y, dir, pb.boardSize);
		} else {
			linkCode = getLinkCode(x + dx, y + dy, getOppDir(dir),
					pb.boardSize);
		}
		pb.drawPath[color].push_back(linkCode);

		// check if we reached the opposite borderline
		int compare = color == RED ? y+dy : x+dx;
		if (compare == pb.boardSize - 1 + kMargin) {
			// we reached the opposite borderline line, save drawPath
			pb.cell[x+dx][y+dy].peg = color;
			pb.drawPath[color].push_back(getAction(x+dx, y+dy, pb.boardSize));
			// copy final drawPath
			b->drawPath[color] = pb.drawPath[color];
			return true;
		}
		nx = x+dx;
		ny = y+dy;
	} else {
		nx = x;
		ny = y;
	}

	//printDrawPath((&pb.drawPath[color]), pb.boardSize, "           >:");

	// search deeper..
	// check surrounding link options...
	for (int i = 0; i < kNumLinkDirections; i++) {
		int nDir = LINKORDER[color][i];
		LinkDescriptor *ld = &(kLinkDescriptorTable[nDir]);
		int ndx = ld->offsets.first;
		int ndy = ld->offsets.second;

		if ((!isPegInDrawPath(&(pb.drawPath[color]), nx + ndx, ny + ndy,
				pb.boardSize))
				&& (pb.cell[nx + ndx][ny + ndy].peg == color
						|| pb.cell[nx + ndx][ny + ndy].peg == EMPTY)
				&& !isOnBorderline(nx + ndx, ny + ndy, 1 - color, pb.boardSize,
						true)
				&& !(pb.cell[nx][ny].blockedLinks & (1UL << nDir))
				&& ! b->cell[nx][ny].deadPeg[color]) {
			// dir doesn't point to a peg we already visited during this exploration (loop)
			// AND (target field is empty OR peg has same color)
			// AND target field is not on opponent's borderline
			// AND link is not blocked
			// AND target peg is no dead end that we detected previously

			if (findDrawPath(b, nx, ny, nDir, color, pb)) {
				return true;
			}
		}
	}
	// there is no draw path including this peg
	b->cell[nx][ny].deadPeg[color] = true;

	return false;
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

void TwixTState::UpdateOutlook(int x, int y) {

	// check for WIN
	bool connectedToStart = isInPegSet(&(mBoard.linkedToStart[mCurrentPlayer]),
			x, y);
	bool connectedToEnd = isInPegSet(&(mBoard.linkedToEnd[mCurrentPlayer]), x,
			y);
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

void TwixTState::ExploreLocalGraph(Board *b, int x, int y, PegSet *pegSet) {

	unsigned int dir;
	int idx;
	// check all linked neighbors
	for (dir = 1, idx = 0; dir <= b->cell[x][y].links; dir <<= 1, idx++) {
		if (b->cell[x][y].links & dir) {
			LinkDescriptor *ld = &(kLinkDescriptorTable[idx]);
			int dx = ld->offsets.first;
			int dy = ld->offsets.second;
			if (!(*pegSet)[(y + dy) * kMaxVirtualBoardSize + (x + dx)]) {
				// linked neighbor is NOT yet member of PegSet => add it and explore
				(*pegSet)[(y + dy) * kMaxVirtualBoardSize + (x + dx)] = true;
				ExploreLocalGraph(b, x + dx, y + dy, pegSet);
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

// helper
std::string coordsToString(int x, int y) {
	string s;
	s = char(int('A') + (x - kMargin)) + to_string(y + 1 - kMargin);
	return s;
}

bool isOnBorderline(int x, int y, int color, int boardSize,
		bool virtualCoords) {

	int margin = virtualCoords ? kMargin : 0;
	if (color == RED) {
		return ((y == margin || y == margin + boardSize - 1)
				&& (x > margin && x < margin + boardSize - 1));
	} else {
		return ((x == margin || x == margin + boardSize - 1)
				&& (y > margin && y < margin + boardSize - 1));
	}
}

bool isOnMargin(int x, int y, int boardSize, bool virtualCoords) {

	int margin = virtualCoords ? kMargin : 0;

	return (y < margin || y > margin + boardSize - 1 || x < margin
			|| x > margin + boardSize - 1 ||
			// corner case
			((x == margin || x == margin + boardSize - 1)
					&& (y == margin || y == margin + boardSize - 1)));
}

bool isInPegSet(PegSet *p, int x, int y) {
	return (*p)[y * kMaxVirtualBoardSize + x];
}

void addToPegSet(PegSet *p, int x, int y) {
	(*p)[y * kMaxVirtualBoardSize + x] = true;
}

void appendBeforeRow(Board *b, string *s, int x, int y) {

	// -1, -1
	int len = s->length();
	appendLinkChar(b, s, x - 1, y, LinkDirections::ENE, "/");
	appendLinkChar(b, s, x - 1, y + 1, LinkDirections::NNE, "/");
	appendLinkChar(b, s, x, y, LinkDirections::WNW, "_");
	if (len == s->length())
		s->append(" ");

	//  0, -1
	len = s->length();
	appendLinkChar(b, s, x, y, LinkDirections::NNE, "|");
	if (len == s->length())
		appendLinkChar(b, s, x, y, LinkDirections::NNW, "|");
	if (len == s->length())
		s->append(" ");

	// +1, -1
	len = s->length();
	appendLinkChar(b, s, x + 1, y, LinkDirections::WNW, "\\");
	appendLinkChar(b, s, x + 1, y + 1, LinkDirections::NNW, "\\");
	appendLinkChar(b, s, x, y, LinkDirections::ENE, "_");
	if (len == s->length())
		s->append(" ");

}

void appendPegRow(Board *b, string *s, int x, int y) {

	// -1, 0
	int len = s->length();
	appendLinkChar(b, s, x - 1, y + 1, LinkDirections::NNE, "|");
	appendLinkChar(b, s, x, y, LinkDirections::WSW, "_");
	if (len == s->length())
		s->append(" ");

	// 0,  0
	if ((x - kMargin == 0 || x - kMargin == b->boardSize - 1)
			&& (y - kMargin == 0 || y - kMargin == b->boardSize - 1)) {
		// corner
		s->append(" ");
	} else {
		appendPegChar(b, s, x, y);
	}

	// +1, 0
	len = s->length();
	appendLinkChar(b, s, x + 1, y + 1, LinkDirections::NNW, "|");
	appendLinkChar(b, s, x, y, LinkDirections::ESE, "_");
	if (len == s->length())
		s->append(" ");
}

void appendAfterRow(Board *b, string *s, int x, int y) {

	// -1, +1
	int len = s->length();
	appendLinkChar(b, s, x + 1, y + 1, LinkDirections::WNW, "\\");
	appendLinkChar(b, s, x, y + 1, LinkDirections::NNW, "\\");
	if (len == s->length())
		s->append(" ");

	//  0, +1
	len = s->length();
	appendLinkChar(b, s, x - 1, y + 1, LinkDirections::ENE, "_");
	appendLinkChar(b, s, x + 1, y + 1, LinkDirections::WNW, "_");
	appendLinkChar(b, s, x, y, LinkDirections::SSW, "|");
	if (len == s->length()) {
		appendLinkChar(b, s, x, y, LinkDirections::SSE, "|");
	}
	if (len == s->length())
		s->append(" ");

	// -1, +1
	len = s->length();
	appendLinkChar(b, s, x - 1, y + 1, LinkDirections::ENE, "/");
	appendLinkChar(b, s, x, y + 1, LinkDirections::NNE, "/");
	if (len == s->length())
		s->append(" ");
}

void appendLinkChar(Board *b, string *s, int x, int y, enum LinkDirections dir,
		string c) {
	if (b->cell[x][y].links & (1UL << dir)) {
		if (b->cell[x][y].peg == RED) {
			appendColorString(s, b->ansiColorOutput, kAnsiRed, c);
		} else if (b->cell[x][y].peg == BLUE) {
			appendColorString(s, b->ansiColorOutput, kAnsiBlue, c);
		} else {
			s->append(c);
		}
	}
}

void appendColorString(string *s, bool ansiColorOutput, string colorString,
		string appString) {
	s->append(ansiColorOutput ? colorString : ""); // make it colored
	s->append(appString);
	s->append(ansiColorOutput ? kAnsiDefault : ""); // make it default
}

void appendPegChar(Board *b, string *s, int x, int y) {
	if (b->cell[x][y].peg == RED) {
		// X
		appendColorString(s, b->ansiColorOutput, kAnsiRed, "X");
	} else if (b->cell[x][y].peg == BLUE) {
		// O
		appendColorString(s, b->ansiColorOutput, kAnsiBlue, "O");
	} else if ((x - kMargin == 0 || x - kMargin == b->boardSize - 1)
			&& (y - kMargin == 0 || y - kMargin == b->boardSize - 1)) {
		// corner
		s->append(" ");
	} else if (x - kMargin == 0 || x - kMargin == b->boardSize - 1) {
		// empty . (blue border line)
		appendColorString(s, b->ansiColorOutput, kAnsiBlue, ".");
	} else if (y - kMargin == 0 || y - kMargin == b->boardSize - 1) {
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

int getLinkCode(int x, int y, int dir, int boardSize) {
	return -  (10 * ((y-kMargin) * boardSize + (x-kMargin)) + dir);
}

string linkCodeToString(int linkCode, int boardSize) {

	int dir = -linkCode % 10;

	int x = (-linkCode/10) % boardSize + kMargin;
	int y = (-linkCode/10) / boardSize + kMargin;

	LinkDescriptor *ld = &kLinkDescriptorTable[dir];
	int dx = kLinkDescriptorTable[dir].offsets.first;
	int dy = kLinkDescriptorTable[dir].offsets.second;

	return coordsToString(x, y) + "-" + coordsToString(x+dx, y+dy);

}

void setBlockers(Board *b, int x, int y, int dx, int dy, LinkDescriptor *ld,
		bool checkDrawPathCrossing) {

	for (auto &&entry : ld->blockingLinks) {
		// get offsets for eastern direction
		int dx = entry.first.first;
		int dy = entry.first.second;
		int blockingDir = entry.second;
		int bitmap = 1UL << blockingDir;
		// block this link (eastern)
		b->cell[x + dx][y + dy].blockedLinks |= bitmap;

		// get offsets for western direction
		int ddx = kLinkDescriptorTable[blockingDir].offsets.first;
		int ddy = kLinkDescriptorTable[blockingDir].offsets.second;
		bitmap = 1UL << getOppDir(blockingDir);
		// block this link (western)
		b->cell[x + dx + ddx][y + dy + ddy].blockedLinks |= bitmap;

		//check if link (eastern) is in drawPath
		if (checkDrawPathCrossing) {
			if (isLinkInDrawPath(b, x + dx, y + dy, blockingDir, RED)) {
				// the new link crosses red's drawPath
				b->drawPath[RED].clear();
			}
			if (isLinkInDrawPath(b, x + dx, y + dy, blockingDir, BLUE)) {
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


}  // namespace twixt
}  // namespace open_spiel


