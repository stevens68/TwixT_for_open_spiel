
#include "open_spiel/games/twixt/twixtboard.h"
#include "open_spiel/games/twixt/twixtcell.h"

using namespace std;

namespace open_spiel {
namespace twixt {

// ANSI colors
const string kAnsiRed = "\e[91m";
const string kAnsiBlue = "\e[94m";
const string kAnsiDefault = "\e[0m";


static pair<int, int> operator+(const pair<int, int> & l,const pair<int, int> & r) {
    return { l.first + r.first, l.second + r.second };
};

// helper functions
inline int oppDir(int dir) {
	return (dir + COMPASS_COUNT / 2) % COMPASS_COUNT;
}

inline int oppCand(int cand) {
	return cand < 16 ? cand<<=4 : cand>>=4;
}

inline std::string coordsToString(Tuple c) {
	return "[" + std::to_string(c.first) + "," + std::to_string(c.second) + "]";
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


Board::Board(int size, bool ansiColorOutput) {
	setSize(size);
	setAnsiColorOutput(ansiColorOutput);

	initializeCells();
	initializeLegalActions();
}

void Board::initializeBlockerMap(Tuple c, int dir, LinkDescriptor *ld) {

	Link link = { c, dir };
	for (auto &&entry : ld->blockingLinks) {
		Tuple fromCoords = c + entry.first;
		if (! coordsOffBoard(fromCoords)) {
			LinkDescriptor *oppLd = &(kLinkDescriptorTable[entry.second]);
			Tuple toCoords = c + entry.first + oppLd->offsets;
			if (! coordsOffBoard(toCoords)) {
				pushBlocker(link, { fromCoords, entry.second });
				pushBlocker(link, { toCoords, oppDir(entry.second) });
			}
		}
	}
}

void Board::updateResult(int player, Tuple c) {

	// check for WIN
	bool connectedToStart = getCell(c)->isLinkedToBorder(player, Border::START);
	bool connectedToEnd = getCell(c)->isLinkedToBorder(player, Border::END);
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

void Board::initializeCells() {

	mCell.resize(getSize());
	clearBlocker();

	// initialize board with color (empty or overboard)
	for (int x = 0; x < getSize(); x++) {
		//adjust dim 2 of board
		mCell[x].resize(getSize());
		for (int y = 0; y < getSize(); y++) {
			// links
			Tuple c = {x, y};
			Cell *pCell = getCell(c);
			// set color to EMPTY or OVERBOARD
			if (coordsOffBoard(c)) {
				pCell->setColor(OVERBOARD);
			} else { // regular board
				pCell->setColor(EMPTY);
				if (x == 0) {
					pCell->setLinkedToBorder(PLAYER_BLUE, Border::START);
				} else if (x == getSize()-1) {
					pCell->setLinkedToBorder(PLAYER_BLUE, Border::END);
				} else if (y == 0) {
					pCell->setLinkedToBorder(PLAYER_RED, Border::START);
				} else if (y == getSize()-1) {
					pCell->setLinkedToBorder(PLAYER_RED, Border::END);
				}

				initializeCandidates(c, pCell);
			}
		}
	}
}

void Board::initializeCandidates(Tuple c, Cell *pCell) {

	for (int dir = 0; dir < COMPASS_COUNT; dir++) {
		LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
		Tuple tc = c + ld->offsets;
		if (! coordsOffBoard(tc)) {
			initializeBlockerMap(c, dir, ld);
			pCell->setNeighbor(dir, tc);
			Cell *pTargetCell = getCell(tc);
			if (! (coordsOnBorder(PLAYER_RED, c) && coordsOnBorder(PLAYER_BLUE, tc)) &&
				! (coordsOnBorder(PLAYER_BLUE, c) && coordsOnBorder(PLAYER_RED, tc))) {
					pCell->setCandidate(PLAYER_RED, dir);
					pCell->setCandidate(PLAYER_BLUE, dir);
			}
		}
	}
}

void Board::initializeLegalActions() {

	for (int player = PLAYER_RED; player < PLAYER_COUNT; player++) {
		vector<Action> *la = &mLegalActions[player];

		la->clear();
		la->reserve(getSize() * getSize());

		for (int y = 0; y < getSize(); y++) {
			for (int x = 0; x < getSize(); x++) {
				int action = y * getSize() + x;
				if ( ! coordsOnBorder(1 - player, {x, y})
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

	for (int y = getSize() -1; y >= 0; y--) {
		// print "before" row
		s.append("    ");
		for (int x = 0; x < getSize(); x++) {
			appendBeforeRow(&s, {x, y});
		}
		s.append("\n");

		// print peg row
		getSize() - y < 10 ? s.append("  ") : s.append(" ");
		appendColorString(&s, kAnsiBlue, to_string(getSize() - y) + " ");
		for (int x = 0; x < getSize(); x++) {
			appendPegRow(&s, {x, y});
		}
		s.append("\n");

		// print "after" row
		s.append("    ");
		for (int x = 0; x < getSize(); x++) {
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

void Board::createTensor(int player, vector<double> *values) const {

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
		for (int y = 0; y < getSize(); y++) {
			for (int x = 1; x < getSize() - 1; x++) {
				if (getConstCell({x,y})->getColor() == COLOR_RED &&
					getConstCell({x,y})->hasLink(dir) )  {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	} else {
		/* PLAYER_BLUE => turn board 90° clockwise */
		for (int y = getSize() - 2; y > 0; y--) {
			for (int x = 0; x < getSize(); x++) {
				if (getConstCell({x,y})->getColor() == COLOR_BLUE
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
		for (int y = 0; y < getSize(); y++) {
			for (int x = 1; x < getSize() - 1; x++) {
				if (getConstCell({x,y})->getColor() == COLOR_RED
						&& ! getConstCell({x,y})->hasLinks()) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	} else {
		/* turn board" 90° clockwise */
		for (int y = getSize() - 2; y > 0; y--) {
			for (int x = 0; x < getSize(); x++) {
				if (getConstCell({x,y})->getColor() == COLOR_BLUE
						&& ! getConstCell({x,y})->hasLinks() ) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	}

}

void Board::appendLinkChar(string *s, Tuple c, enum Compass dir, string linkChar) const {
	if (! coordsOffBoard(c) && getConstCell(c)->hasLink(dir)) {
		if (getConstCell(c)->getColor() == PLAYER_RED) {
			appendColorString(s, kAnsiRed, linkChar);
		} else if (getConstCell(c)->getColor() == PLAYER_BLUE) {
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

void Board::appendPegChar(string *s, Tuple c) const {
	if (getConstCell(c)->getColor() == PLAYER_RED) {
		// X
		appendColorString(s, kAnsiRed, "X");
	} else if (getConstCell(c)->getColor() == PLAYER_BLUE) {
		// O
		appendColorString(s, kAnsiBlue, "O");
	} else if (coordsOffBoard(c)) {
		// corner
		s->append(" ");
	} else if (c.first == 0 || c.first == getSize() - 1) {
		// empty . (blue border line)
		appendColorString(s, kAnsiBlue, ".");
	} else if (c.second == 0 || c.second == getSize() - 1) {
		// empty . (red border line)
		appendColorString(s, kAnsiRed, ".");
	} else {
		// empty (non border line)
		s->append(".");
	}
}

void Board::appendBeforeRow(string *s, Tuple c) const {

	// -1, +1
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, 0}, Compass::ENE, "/");
	appendLinkChar(s, c + (Tuple) {-1,-1}, Compass::NNE, "/");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::WNW, "_");
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
	appendLinkChar(s, c + (Tuple) {+1, 0}, Compass::WNW, "\\");
	appendLinkChar(s, c + (Tuple) {+1,-1}, Compass::NNW, "\\");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::ENE, "_");
	if (len == s->length())
		s->append(" ");

}

void Board::appendPegRow(string *s, Tuple c) const {

	// -1, 0
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {-1,-1}, Compass::NNE, "|");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::WSW, "_");
	if (len == s->length())
		s->append(" ");

	//  0,  0
	appendPegChar(s, c);

	// +1, 0
	len = s->length();
	appendLinkChar(s, c + (Tuple) {+1,-1}, Compass::NNW, "|");
	appendLinkChar(s, c + (Tuple) { 0, 0}, Compass::ESE, "_");
	if (len == s->length())
		s->append(" ");

}

void Board::appendAfterRow(string *s, Tuple c) const {

	// -1, -1
	int len = s->length();
	appendLinkChar(s, c + (Tuple) {+1, -1}, Compass::WNW, "\\");
	appendLinkChar(s, c + (Tuple) { 0, -1}, Compass::NNW, "\\");
	if (len == s->length())
		s->append(" ");

	//  0, -1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, -1}, Compass::ENE, "_");
	appendLinkChar(s, c + (Tuple) {+1, -1}, Compass::WNW, "_");
	appendLinkChar(s, c, Compass::SSW, "|");
	if (len == s->length()) {
		appendLinkChar(s, c, Compass::SSE, "|");
	}
	if (len == s->length())
		s->append(" ");

	// -1, -1
	len = s->length();
	appendLinkChar(s, c + (Tuple) {-1, -1}, Compass::ENE, "/");
	appendLinkChar(s, c + (Tuple) { 0, -1}, Compass::NNE, "/");
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
			initializeCells();
			initializeLegalActions();

		} else {
			// not swapped: regular move
			// remove move #1 from legal moves
			removeLegalAction(PLAYER_RED, getMoveOne());
			removeLegalAction(PLAYER_BLUE, getMoveOne());
		}
	}

	// calculate virtual coords from move
	Tuple c = { (int) move % getSize(), (int) move / getSize() };

	// set links

	setPegAndLinks(player, c);

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


void Board::setPegAndLinks(int player, Tuple c) {

	bool linkedToNeutral = false;
	bool linkedToStart = false;
	bool linkedToEnd = false;

	// set peg
	Cell *pCell = getCell(c);
	pCell->setColor(player);

	int dir=0;
	for (int cand=1, dir=0; cand <= pCell->getCandidates(player) ; cand<<=1, dir++) {
		if (pCell->isCandidate(player, cand)) {
			Cell *pTargetCell = getCell(pCell->getNeighbor(dir));
			if (pTargetCell->getColor() == EMPTY) {
				// pCell is not a candidate for pTargetCell from opponent's persp. anymore
				//cout << "**** about to delete candidate " << to_string(oppDir(dir)) << " from " << coordsToString(pCell->getNeighbor(dir)) << endl;
				//cout << "Before: " << to_string(pTargetCell->getCandidates(1-player)) << endl;

				pTargetCell->deleteCandidate(1-player, oppCand(cand));
				//cout << "After:  " << to_string(pTargetCell->getCandidates(1-player)) << endl;


			} else {
				//cout << "**** about link to " << coordsToString(pCell->getNeighbor(dir)) << endl;
				// set link
				pCell->setLink(dir);
				pTargetCell->setLink(oppDir(dir));

				// set blockers
				vector<Link> *blockers = getBlockers((Link) {c, dir});
				for (auto &&bl : *blockers) {
					//cout << "       blocking: "  << coordsToString(bl.first) << ": " << to_string(bl.second) << endl;
					getCell(bl.first)->deleteCandidate(bl.second);
				}

				// check if cell we link to is linked to START / END
				if (pTargetCell->isLinkedToBorder(player, Border::START)) {
					pCell->setLinkedToBorder(player, Border::START);
					linkedToStart = true;
				}
				if (pTargetCell->isLinkedToBorder(player, Border::END)) {
					pCell->setLinkedToBorder(player, Border::END);
					linkedToEnd = true;
				}
				if (! pTargetCell->isLinkedToBorder(player, Border::START) &&
					! pTargetCell->isLinkedToBorder(player, Border::END) ) {
					linkedToNeutral = true;
				}
			}
		}
	}

	//check if we need to explore further
	if (pCell->isLinkedToBorder(player, Border::START) && linkedToNeutral) {

		// case: new cell is linked to START and linked to neutral cells
		// => explore neutral graph and add all its cells to START
		exploreLocalGraph(player, pCell, Border::START);
	}
	if (pCell->isLinkedToBorder(player, Border::END) && linkedToNeutral) {
		// case: new cell is linked to END and linked to neutral cells
		// => explore neutral graph and add all its cells to END
		exploreLocalGraph(player, pCell, Border::END);
	}

}

void Board::exploreLocalGraph(int player, Cell *pCell, enum Border border) {

	int dir=0;
	for (int link=1, dir=0; link <= pCell->getLinks(); link<<=1, dir++) {
		if (pCell->isLinked(link)) {
			Cell *pTargetCell = getCell(pCell->getNeighbor(dir));
			if (! pTargetCell->isLinkedToBorder(player, border)) {
				// linked neighbor is NOT yet member of PegSet
				// => add it and explore
				pTargetCell->setLinkedToBorder(player, border);
				exploreLocalGraph(player, pTargetCell, border);
			}
		}
	}
}


bool Board::coordsOnBorder(int player, Tuple c) const {

	if (player == PLAYER_RED) {
		return ((c.second == 0 || c.second == getSize() - 1)
				&& (c.first > 0 && c.first < getSize() - 1));
	} else {
		return ((c.first == 0 || c.first == getSize() - 1)
				&& (c.second > 0 && c.second < getSize() - 1));
	}
}

bool Board::coordsOffBoard(Tuple c) const {

	return (c.second < 0 || c.second > getSize() - 1 ||
			c.first  < 0 || c.first >  getSize() - 1 ||
			// corner case
		   ((c.first  == 0 || c.first  == getSize() - 1) &&
		    (c.second == 0 || c.second == getSize() - 1)));
}


}  // namespace twixt
}  // namespace open_spiel

