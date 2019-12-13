
#include "open_spiel/games/twixt/twixtboard.h"

using namespace std;

namespace open_spiel {
namespace twixt {


const int LINKORDER[PLAYER_COUNT][COMPASS_COUNT] = {
	{7, 0, 6, 1, 5, 2, 4, 3},
	{2, 1, 3, 0, 4, 7, 5, 6}
};

// ANSI colors
const string kAnsiRed = "\e[91m";
const string kAnsiBlue = "\e[94m";
const string kAnsiDefault = "\e[0m";

static pair<int, int> operator+(const pair<int, int> & l,const pair<int, int> & r) {
    return { l.first + r.first, l.second + r.second };
};


// helper functions
inline string coordsToString(Coordinates, int);
string linkCodeToString(int, int);

// helper ****************************************

inline int getOppDir(int dir) {
	return (dir + COMPASS_COUNT / 2) % COMPASS_COUNT;
}

inline int getLinkCode(Coordinates c, int dir, int boardSize) {
	return -  (10 * ((c.second-kMargin) * boardSize + (c.first-kMargin)) + dir);
}

inline Coordinates getNeighbour(Coordinates c, Coordinates offset) {
	return c + offset;
}

inline std::string coordsToString(Coordinates c, int boardSize) {
	string s;
	s = char(int('A') + (c.first - kMargin)) + to_string(boardSize + kMargin - c.second);
	return s;
}


// ***********************************************

// table of 8 link descriptors
static vector<LinkDescriptor> kLinkDescriptorTable
{
	// NNE
	{
	   {1,  2},   // offset of target peg (2 up, 1 right)
	   {           // blocking/blocked links
			{{0,  1}, Compass::ENE },
			{{-1, 0}, Compass::ENE },

			{{ 0,  2}, Compass::ESE },
			{{ 0,  1}, Compass::ESE },
			{{-1,  2}, Compass::ESE },
			{{-1,  1}, Compass::ESE },

			{{0,  1}, Compass::SSE },
			{{0,  2}, Compass::SSE },
			{{0,  3}, Compass::SSE }
	   }
	},
	// ENE
	{
	   {2,  1},    // offset of target peg
	   {           // blocking/blocked links
			{{ 0, -1}, Compass::NNE },
			{{ 1,  0}, Compass::NNE },

			{{-1,  1}, Compass::ESE },
			{{ 0,  1}, Compass::ESE },
			{{ 1,  1}, Compass::ESE },

			{{ 0,  1}, Compass::SSE },
			{{ 0,  2}, Compass::SSE },
			{{ 1,  1}, Compass::SSE },
			{{ 1,  2}, Compass::SSE }
	   }
	},
	// ESE
	{
	   { 2, -1},   // offset of target peg
	   {           // blocking/blocked links
			{{ 0, -1}, Compass::NNE },
			{{ 1, -1}, Compass::NNE },
			{{ 0, -2}, Compass::NNE },
			{{ 1, -2}, Compass::NNE },

			{{-1, -1}, Compass::ENE },
			{{ 0, -1}, Compass::ENE },
			{{ 1, -1}, Compass::ENE },

			{{ 0,  1}, Compass::SSE },
			{{ 1,  0}, Compass::SSE }
	   }
	},
	// SSE
	{
	   { 1, -2},   // offset of target peg
	   {           // blocking/blocked links
			{{ 0, -1}, Compass::NNE },
			{{ 0, -2}, Compass::NNE },
			{{ 0, -3}, Compass::NNE },

			{{-1, -1}, Compass::ENE },
			{{ 0, -1}, Compass::ENE },
			{{-1, -2}, Compass::ENE },
			{{ 0, -2}, Compass::ENE },

			{{-1,  0}, Compass::ESE },
			{{ 0, -1}, Compass::ESE }
	   }
	},
	// SSW
	{
	   {-1, -2},    // offset of target peg
	   {           // blocking/blocked links
			{{-1, -1}, Compass::ENE },
			{{-2, -2}, Compass::ENE },

			{{-2,  0}, Compass::ESE },
			{{-1,  0}, Compass::ESE },
			{{-2, -1}, Compass::ESE },
			{{-1, -1}, Compass::ESE },

			{{-1,  1}, Compass::SSE },
			{{-1,  0}, Compass::SSE },
			{{-1, -1}, Compass::SSE }
	   }
	},
	// WSW
	{
	   {-2, -1},   // offset of target peg
	   {           // blocking/blocked links
			{{-2, -2}, Compass::NNE },
			{{-1, -1}, Compass::NNE },

			{{-3,  0}, Compass::ESE },
			{{-2,  0}, Compass::ESE },
			{{-1,  0}, Compass::ESE },

			{{-2,  1}, Compass::SSE },
			{{-1,  1}, Compass::SSE },
			{{-2,  0}, Compass::SSE },
			{{-1,  0}, Compass::SSE }
	   }
	},
	// WNW
	{
	   {-2, 1},   // offset of target peg
	   {           // blocking/blocked links
			{{-2,  0}, Compass::NNE },
			{{-1,  0}, Compass::NNE },
			{{-2, -1}, Compass::NNE },
			{{-1, -1}, Compass::NNE },

			{{-3,  0}, Compass::ENE },
			{{-2,  0}, Compass::ENE },
			{{-1,  0}, Compass::ENE },

			{{-2,  2}, Compass::SSE },
			{{-1,  1}, Compass::SSE }
	   }
	},
	// NNW
	{
	   {-1, 2},   // offset of target peg
	   {           // blocking/blocked links
			{{-1,  1}, Compass::NNE },
			{{-1,  0}, Compass::NNE },
			{{-1, -1}, Compass::NNE },

			{{-2,  1}, Compass::ENE },
			{{-1,  1}, Compass::ENE },
			{{-2,  0}, Compass::ENE },
			{{-1,  0}, Compass::ENE },

			{{-2,  2}, Compass::ESE },
			{{-1,  1}, Compass::ESE }
	   }
	}

};


string linkCodeToString(int linkCode, int boardSize) {

	int dir = -linkCode % 10;
	Coordinates c = { (-linkCode/10) % boardSize + kMargin, (-linkCode/10) / boardSize + kMargin };

	LinkDescriptor *ld = &kLinkDescriptorTable[dir];
	Coordinates cd = kLinkDescriptorTable[dir].offsets;

	return coordsToString(c, boardSize) + "-" + coordsToString(c + cd, boardSize);

}


Board::Board(int size, bool ansiColorOutput) {
	setSize(size);
	setAnsiColorOutput(ansiColorOutput);
	setVirtualSize(size + 2 * kMargin);

	mCell.resize(getVirtualSize());

	initialize();
}

void Board::initialize() {

	initializePegs();
	initializeLegalActions();

}

void Board::updateResult(int player, Coordinates c) {

	// check for WIN
	bool connectedToStart = isInPegSet(player, Border::START, c);
	bool connectedToEnd = isInPegSet(player, Border::END, c);
	if (connectedToStart && connectedToEnd) {
		// peg is linked to both boarder lines
		setResult(player == PLAYER_RED ? Result::RED_WON : Result::BLUE_WON);
		return;
	}

	// check if we are early in the game...
	if (getMoveCounter() < getSize() - 1) {
		// less than 5 moves played on a 6x6 board => no win or draw possible, no need to update
		return;
	}

	//check if opponent (player to turn next) has any legal moves left
	if (! hasLegalActions(1 - player)) {
		setResult(Result::DRAW);
		return;
	}

}


void Board::setBlockers(Coordinates c, LinkDescriptor *ld) {

	for (auto &&entry : ld->blockingLinks) {
		// get offsets for eastern direction
		Coordinates cd = entry.first;
		int blockingDir = entry.second;

		// block this link (eastern)
		getCell(c + cd)->setBlocked(blockingDir);
		// block this link (western)
		Coordinates cdd = kLinkDescriptorTable[blockingDir].offsets;
		getCell(c + cd + cdd)->setBlocked(getOppDir(blockingDir));

	}

}

void Board::initializePegs() {

	int virtualBoardSize = getSize() + 2 * kMargin;

	//adjust dim 1 of board
	mCell.resize(virtualBoardSize);

	//  initialize peg sets to false (2 sets per player)
	for (int ii = 0; ii < kMaxVirtualBoardSize * kMaxVirtualBoardSize; ii++) {
		mLinkedToBorder[PLAYER_RED][Border::START][ii] = false;
		mLinkedToBorder[PLAYER_RED][Border::END][ii] = false;
		mLinkedToBorder[PLAYER_BLUE][Border::START][ii] = false;
		mLinkedToBorder[PLAYER_BLUE][Border::END][ii] = false;
	}

	// initialize board with pegs (empty or overboard)
	for (int x = 0; x < virtualBoardSize; x++) {
		//adjust dim 2 of board
		mCell[x].resize(virtualBoardSize);

		for (int y = 0; y < virtualBoardSize; y++) {
			// links
			Coordinates c = {x, y};
			getCell(c)->clearLinks();
			getCell(c)->clearBlockedLinks();
			// set pegs to EMPTY, PLAYER_RED or PLAYER_BLUE depending on boardType and position
			if (coordsOffBoard(c)) {
				getCell(c)->setPeg(OVERBOARD);
			} else { // regular board
				getCell(c)->setPeg(EMPTY);
			} // if
		}
	}

	// flag impossible links as blocked links
	for (int x = kMargin; x < getSize()+kMargin; x++) {
		for (int y = kMargin; y < getSize()+kMargin; y++) {
			for (int dir = 0; dir < COMPASS_COUNT; dir++) {
				LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
				Coordinates c = { x, y };
				Coordinates tc = c + ld->offsets;
				if (getCell(tc)->getPeg() != Color::EMPTY) {
					getCell(c)->setBlocked(dir);
				} else if (coordsOnBorderline(PLAYER_RED, tc)){
					getCell(c)->setBlocked(PLAYER_RED, dir);
				} else if (coordsOnBorderline(PLAYER_BLUE, tc)){
					getCell(c)->setBlocked(PLAYER_BLUE, dir);
				}
			}
		}
	}

}

void Board::initializeLegalActions() {


	for (int player = PLAYER_RED; player < PLAYER_COUNT; player++) {
		vector<Action> *la = &mLegalActions[player];

		la->clear();
		la->reserve(getSize() * getSize());

		for (int y = kMargin; y < getSize()+kMargin; y++) {
			for (int x = kMargin; x < getSize()+kMargin; x++) {
				int action = (y-kMargin) * getSize() + (x-kMargin);
				if ( ! coordsOnBorderline(1 - player, {x, y})
						&& ! coordsOffBoard({x, y})) {
					mLegalActionIndex[player][action] = la->size();
					la->push_back(action);
				} else {
					mLegalActionIndex[player][action] = -1;
				}
			}
		}
	}
}

string Board::toString() const {

	string s = "";

	// head line
	s.append("     ");
	for (int y = 0; y < getSize(); y++) {
		string letter = "";
		letter += char(int('A') + y);
		letter += "  ";
		appendColorString(&s, kAnsiRed, letter);
	}
	s.append("\n");

	for (int y = getSize() -1 + kMargin; y >= kMargin; y--) {
		// print "before" row
		s.append("    ");
		for (int x = 0 + kMargin; x < getSize() + kMargin; x++) {
			appendBeforeRow(&s, {x, y});
		}
		s.append("\n");

		// print peg row
		getSize() + kMargin - y < 10 ? s.append("  ") : s.append(" ");
		appendColorString(&s, kAnsiBlue, to_string(getSize() + kMargin - y) + " ");
		for (int x = 0 + kMargin; x < getSize() + kMargin; x++) {
			appendPegRow(&s, {x, y});
		}
		s.append("\n");

		// print "after" row
		s.append("    ");
		for (int x = 0 + kMargin; x < getSize() + kMargin; x++) {
			appendAfterRow(&s, {x, y});
		}
		s.append("\n");
	}
	s.append("\n");

	if (mSwapped)
		s.append("[swapped]");

	switch (mResult) {
	case Result::OPEN:
		break;
	case Result::RED_WON:
		s.append("[X has won]");
		break;
	case Result::BLUE_WON:
		s.append("[O has won]");
		break;
	case Result::DRAW:
		s.append("[draw]");
	default:
		break;
	}

		/* debug >>>>
	s.append("\n");
	for (int player = PLAYER_RED; player < PLAYER_COUNT; player++) {
		vector<Action>::iterator it;
		vector<Action> *v = getLegalActions(player);
		for (it = v->begin(); it != v->end(); it++) {
			s.append(to_string(*it));
			s.append(" ");
		}
		s.append("\n");
	}
	s.append("\n");
	<<< debug */

	return s;
}

string Board::actionToString(int player, Action move) const {

	string s = "";
	s += char(int('A') + move % getSize());
	s.append(to_string(getSize() - move / getSize()));
	return s;
}

void Board::createNormalizedVector(int player, vector<double> *values) const {

	values->resize(0);
	values->reserve(kNumPlanes * getSize() * (getSize() - 2));

	for (int player = PLAYER_RED; player < PLAYER_COUNT; player++) {
		// add peg configuration
		addPegPlane(player, values);

		// add link configuration
		for (int dir = 0; dir < (COMPASS_COUNT / 2); dir++) {
			addLinkPlane(player, dir, values);
		}
	}

	// add plane that indicates who's turn it is
	addBinaryPlane(player, values);

}

// Adds a binary plane to the information state vector - all ones or all zeros
void Board::addBinaryPlane(int player, std::vector<double> *values) const {
	double normalizedVal = static_cast<double>(player);
	values->insert(values->end(), getSize() * (getSize() - 2), normalizedVal);
}

// Adds a plane to the information state vector corresponding to the presence
// and absence of the given link at each cell
void Board::addLinkPlane(int player, int dir, std::vector<double> *values) const {

	if (player == PLAYER_RED) {
		for (int y = 0 + kMargin; y < getSize() + kMargin; y++) {
			for (int x = 1 + kMargin; x < getSize() - 1 + kMargin; x++) {
				if (getConstCell({x,y})->getPeg() == COLOR_RED &&
					getConstCell({x,y})->hasLink(dir) )  {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	} else {
		/* PLAYER_BLUE => turn board 90° clockwise */
		for (int y = getSize() - 2 + kMargin; y > 0 + kMargin; y--) {
			for (int x = 0 + kMargin; x < getSize() + kMargin; x++) {
				if (getConstCell({x,y})->getPeg() == COLOR_BLUE
						&& getConstCell({x,y})->hasLink(dir) ) {
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
void Board::addPegPlane(int player, std::vector<double> *values) const {

	if (player == PLAYER_RED) {
		for (int y = 0 + kMargin; y < getSize() + kMargin; y++) {
			for (int x = 1 + kMargin; x < getSize() - 1 + kMargin; x++) {
				if (getConstCell({x,y})->getPeg() == COLOR_RED
						&& ! getConstCell({x,y})->hasLinks()) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	} else {
		/* turn board" 90° clockwise */
		for (int y = getSize() - 2 + kMargin; y > 0 + kMargin; y--) {
			for (int x = 0 + kMargin; x < getSize() + kMargin; x++) {
				if (getConstCell({x,y})->getPeg() == COLOR_BLUE
						&& ! getConstCell({x,y})->hasLinks() ) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	}

}

void Board::appendLinkChar(string *s, Coordinates c, enum Compass dir,
		string linkChar) const {
	if (getConstCell(c)->hasLink(dir)) {
		if (getConstCell(c)->getPeg() == PLAYER_RED) {
			appendColorString(s, kAnsiRed, linkChar);
		} else if (getConstCell(c)->getPeg() == PLAYER_BLUE) {
			appendColorString(s, kAnsiBlue, linkChar);
		} else {
			s->append(linkChar);
		}
	}
}

void Board::appendColorString(string *s, string colorString, string appString) const {

	s->append(getAnsiColorOutput() ? colorString : ""); // make it colored
	s->append(appString);
	s->append(getAnsiColorOutput() ? kAnsiDefault : ""); // make it default
}

void Board::appendPegChar(string *s, Coordinates c) const {
	if (getConstCell(c)->getPeg() == PLAYER_RED) {
		// X
		appendColorString(s, kAnsiRed, "X");
	} else if (getConstCell(c)->getPeg() == PLAYER_BLUE) {
		// O
		appendColorString(s, kAnsiBlue, "O");
	} else if (coordsOffBoard(c)) {
		// corner
		s->append(" ");
	} else if (c.first - kMargin == 0 || c.first - kMargin == getSize() - 1) {
		// empty . (blue border line)
		appendColorString(s, kAnsiBlue, ".");
	} else if (c.second - kMargin == 0 || c.second - kMargin == getSize() - 1) {
		// empty . (red border line)
		appendColorString(s, kAnsiRed, ".");
	} else {
		// empty (non border line)
		s->append(".");
	}
}

void Board::appendBeforeRow(string *s, Coordinates c) const {

	// -1, +1
	int len = s->length();
	appendLinkChar(s, getNeighbour(c, {-1, 0}), Compass::ENE, "/");
	appendLinkChar(s, getNeighbour(c, {-1,-1}), Compass::NNE, "/");
	appendLinkChar(s, getNeighbour(c, { 0, 0}), Compass::WNW, "_");
	if (len == s->length())
		s->append(" ");

	//  0, +1
	len = s->length();
	appendLinkChar(s, c, Compass::NNE, "|");
	if (len == s->length())
		appendLinkChar(s, c, Compass::NNW, "|");
	if (len == s->length())
		s->append(" ");

	// +1, +1
	len = s->length();
	appendLinkChar(s, getNeighbour(c, {+1, 0}), Compass::WNW, "\\");
	appendLinkChar(s, getNeighbour(c, {+1,-1}), Compass::NNW, "\\");
	appendLinkChar(s, getNeighbour(c, { 0, 0}), Compass::ENE, "_");
	if (len == s->length())
		s->append(" ");

}

void Board::appendPegRow(string *s, Coordinates c) const {

	// -1, 0
	int len = s->length();
	appendLinkChar(s, getNeighbour(c, {-1,-1}), Compass::NNE, "|");
	appendLinkChar(s, getNeighbour(c, { 0, 0}), Compass::WSW, "_");
	if (len == s->length())
		s->append(" ");

	//  0,  0
	appendPegChar(s, c);

	// +1, 0
	len = s->length();
	appendLinkChar(s, c + (Coordinates) {+1,-1}, Compass::NNW, "|");
	appendLinkChar(s, c + (Coordinates) { 0, 0}, Compass::ESE, "_");
	if (len == s->length())
		s->append(" ");

}

void Board::appendAfterRow(string *s, Coordinates c) const {

	// -1, -1
	int len = s->length();
	appendLinkChar(s, getNeighbour(c, {+1, -1}), Compass::WNW, "\\");
	appendLinkChar(s, getNeighbour(c, { 0, -1}), Compass::NNW, "\\");
	if (len == s->length())
		s->append(" ");

	//  0, -1
	len = s->length();
	appendLinkChar(s, getNeighbour(c, {-1, -1}), Compass::ENE, "_");
	appendLinkChar(s, getNeighbour(c, {+1, -1}), Compass::WNW, "_");
	appendLinkChar(s, c, Compass::SSW, "|");
	if (len == s->length()) {
		appendLinkChar(s, c, Compass::SSE, "|");
	}
	if (len == s->length())
		s->append(" ");

	// -1, -1
	len = s->length();
	appendLinkChar(s, getNeighbour(c, {-1, -1}), Compass::ENE, "/");
	appendLinkChar(s, getNeighbour(c, { 0, -1}), Compass::NNE, "/");
	if (len == s->length())
		s->append(" ");
}

void Board::applyAction(int player, Action move) {


	if (getMoveCounter() == 1) {
		if (move == getMoveOne()) {
			// player 2 swapped
			setSwapped(true);
			// turn move 90° clockwise
			move = (move % getSize()) * getSize()
					+ (getSize() - (move / getSize()) - 1);

			// undo first move (peg and legal actions)
			initialize();
		} else {
			// not swapped: regular move
			// remove move #1 from legal moves
			removeLegalAction(PLAYER_RED, getMoveOne());
			removeLegalAction(PLAYER_BLUE, getMoveOne());
		}
	}

	// calculate virtual coords from move
	Coordinates c = {
			(int) move % getSize() + kMargin,
			(int) move / getSize() + kMargin
	};

	// set peg on regular board
	getCell(c)->setPeg(player);

	// if peg is on borderline, add it to respective peg list
	int compare = player == PLAYER_RED ? c.second : c.first;
	if (compare - kMargin == 0) {
		addToPegSet(player, Border::START, c);
	} else if (compare - kMargin == getSize() - 1) {
		addToPegSet(player, Border::END, c);
	}

	// set links
	setSurroundingLinks(player, c);

	incMoveCounter();

	if (getMoveCounter() == 1) {
		// do not remove the move from legal actions but store it
		// because player 2 might want to swap
		setMoveOne(move);
	} else {
		// remove move one from mLegalActions
		removeLegalAction(PLAYER_RED, move);
		removeLegalAction(PLAYER_BLUE, move);
	}

	// Update the predicted result and update mCurrentPlayer...
	updateResult(player, c);

}

void Board::setSurroundingLinks(int player, Coordinates c) {

	bool linkedToNeutral = false;
	bool linkedToStart = false;
	bool linkedToEnd = false;

	for (int dir = 0; dir < COMPASS_COUNT; dir++) {
		if (! getCell(c)->isBlocked(player, dir) ) {
			// link is not blocked
			LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
			Coordinates cd = ld->offsets;
			if (getCell(c + cd)->getPeg() == getCell(c)->getPeg()) {
				// target peg has same color and link is not blocked
				// set link from/to this peg
				getCell(c)->setLink(dir);
				getCell(c + cd)->setLink(getOppDir(dir));

				setBlockers(c, ld);

				// check if peg we link to is in a pegset (START / END)
				if (isInPegSet(player, Border::START, c + cd)) {
					addToPegSet(player, Border::START, c);
					linkedToStart = true;
				}
				if (isInPegSet(player, Border::END, c + cd)) {
					addToPegSet(player, Border::END, c);
					linkedToEnd = true;
				}
				if (! isInPegSet(player, Border::START, c + cd) &&
					! isInPegSet(player, Border::END, c + cd)) {
					linkedToNeutral = true;
				}

			}
		}
	}

	//check if we need to explore further
	if (isInPegSet(player, Border::START, c) && linkedToNeutral) {

		// case: new peg is in PegSet START and linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet START
		exploreLocalGraph(player, c, Border::START);
	}
	if (isInPegSet(player, Border::END, c) && linkedToNeutral) {
		// case: new peg is in PegSet END and is linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet END
		exploreLocalGraph(player, c, Border::END);
	}

}

void Board::exploreLocalGraph(int player, Coordinates c, enum Border border) {

	// check all linked neighbors
	for (int dir = 0; dir < COMPASS_COUNT; dir++) {
		if (getCell(c)->hasLink(dir)) {
			LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
			Coordinates cd = ld->offsets;
			if (! isInPegSet(player, border, c + cd)) {
				// linked neighbor is NOT yet member of PegSet
				// => add it and explore
				addToPegSet(player, border, c + cd);
				exploreLocalGraph(player, c + cd, border);
			}
		}
	}
}


bool Board::coordsOnBorderline(int player, Coordinates c) const {

	if (player == PLAYER_RED) {
		return ((c.second == kMargin || c.second == kMargin + getSize() - 1)
				&& (c.first > kMargin && c.first < kMargin + getSize() - 1));
	} else {
		return ((c.first == kMargin || c.first == kMargin + getSize() - 1)
				&& (c.second > kMargin && c.second < kMargin + getSize() - 1));
	}
}

bool Board::coordsOffBoard(Coordinates c) const {

	return (c.second < kMargin || c.second > kMargin + getSize() - 1 ||
			c.first  < kMargin || c.first >  kMargin + getSize() - 1 ||
			// corner case
		   ((c.first  == kMargin || c.first  == kMargin + getSize() - 1) &&
		    (c.second == kMargin || c.second == kMargin + getSize() - 1)));
}


}  // namespace twixt
}  // namespace open_spiel

