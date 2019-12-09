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


// ***  helper **************************************************************


int getAction(Coordinates c, int boardSize) {
	return (c.second - kMargin) * boardSize + (c.first - kMargin);
}


std::string coordsToString(Coordinates c, int boardSize) {
	string s;
	s = char(int('A') + (c.first - kMargin)) + to_string(boardSize + kMargin - c.second);
	return s;
}

bool isOnBorderline(int color, Coordinates c, int boardSize) {

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

void addToPegSet(PegSet *p, Coordinates c) {
	(*p)[c.second * kMaxVirtualBoardSize + c.first] = true;
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



Coordinates getNeighbour(Coordinates c, Coordinates offset) {
	return c + offset;
}

// ***********************************************

int Cell::getPeg() const {
	return mPeg;
}

void Cell::setPeg(int color) {
	mPeg = color;;
}

void Cell::clearLinks() {
	mLinks = 0;
}

void Cell::clearBlockedLinks() {
	mBlockedLinks = 0;
}

void Cell::setLink(int dir) {
	mLinks |= 1UL << dir;
}

void Cell::setBlocked(int dir) {
	mBlockedLinks |= 1UL << dir;
}

bool Cell::hasLink(int dir) const {
	return mLinks & 1UL << dir;
}

bool Cell::hasLinks() const {
	return mLinks > 0;
}

bool Cell::isBlocked(int dir) const {
	return mBlockedLinks & 1UL << dir;
}

// ***********************************************************

Board::~Board() {
}

Board::Board() {
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

	initializeLegalActions(RED);
	initializeLegalActions(BLUE);

}

void Board::updateOutlook(int color, Coordinates c) {

	// check for WIN
	bool connectedToStart = isInPegSet(color, Border::START, c);
	bool connectedToEnd = isInPegSet(color, Border::END, c);
	if (connectedToStart && connectedToEnd) {
		// peg is linked to both boarder lines
		setOutlook(color == RED ? Outlook::RED_WON : Outlook::BLUE_WON);
		return;
	}

	// check if we are early in the game...
	if (getMoveCounter() < getSize() - 1) {
		// less than 5 moves played on a 6x6 board => no win or draw possible, no need to update
		return;
	}

	//check if opponent (player to turn next) has any legal moves left
	if (! hasLegalActions(1 - color)) {
		setOutlook(Outlook::DRAW);
		return;
	}
}


void Board::setSize(int size) {
	mSize = size;
}

int Board::getSize() const {
	return mSize;
}

void Board::setVirtualSize(int virtualSize) {
	mVirtualSize = virtualSize;
}

int Board::getVirtualSize() const {
	return mVirtualSize;
}

bool Board::getAnsiColorOutput() const {
	return mAnsiColorOutput;
}

void Board::setAnsiColorOutput(bool ansiColorOutput) {
	mAnsiColorOutput = ansiColorOutput;
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


bool Board::isInPegSet(int color, enum Border border, Coordinates c) const {
	return mLinkedToBorder[color][border][c.second * getVirtualSize() + c.first];
}


void Board::addToPegSet(int color, enum Border border, Coordinates c) {
	mLinkedToBorder[color][border][c.second * getVirtualSize() + c.first] = true;
}

Cell* Board::getCell(Coordinates c) {
	return  &(mCell[c.first][c.second]);
}

const Cell* Board::getConstCell(Coordinates c)  const {
	return  &(mCell[c.first][c.second]);
}


void Board::initializePegs() {

	int virtualBoardSize = getSize() + 2 * kMargin;

	//adjust dim 1 of board
	mCell.resize(virtualBoardSize);

	//  initialize peg sets to false (2 sets per player)
	for (int ii = 0; ii < kMaxVirtualBoardSize * kMaxVirtualBoardSize; ii++) {
		mLinkedToBorder[RED][Border::START][ii] = false;
		mLinkedToBorder[RED][Border::END][ii] = false;
		mLinkedToBorder[BLUE][Border::START][ii] = false;
		mLinkedToBorder[BLUE][Border::END][ii] = false;
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
			// set pegs to EMPTY, RED or BLUE depending on boardType and position
			if (isOnMargin(c, getSize())) {
				getCell(c)->setPeg(OVERBOARD);
			} else { // regular board
				getCell(c)->setPeg(EMPTY);
			} // if
		}
	}

}

void Board::initializeLegalActions(int color) {

	vector<Action> *la = &mLegalActions[color];

	la->clear();
	la->reserve(getSize() * getSize());

	for (int y = 0; y < getVirtualSize(); y++) {
		for (int x = 0; x < getVirtualSize(); x++) {
			if (!isOnBorderline(1 - color, {x, y}, getSize())
					&& !isOnMargin({x, y}, getSize())) {
				int action = (y-kMargin) * getSize() + (x-kMargin);
				la->push_back(action);
			}
		}
	}
}

bool Board::hasLegalActions(int color) const {
	return mLegalActions[color].size() > 0;
}

vector<Action> Board::getLegalActions(int color) const {
	return mLegalActions[color];
}

void Board::removeLegalAction(int color, Action action) {

	vector<Action> *la = &mLegalActions[color];
	vector<Action>::iterator it;
    it = find(la->begin(), la->end(), action);
    if (it != la->end()) la->erase(it);

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
		y - kMargin + 1 < 10 ? s.append("  ") : s.append(" ");
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

		/* debug >>>>
	s.append("\n");
	for (int color = RED; color <= BLUE; color++) {
		vector<Action>::iterator it;
		vector<Action> *v = getLegalActions(color);
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

void Board::createNormalizedVector(int color, vector<double> *values) const {

	values->resize(0);
	values->reserve(kNumPlanes * getSize() * (getSize() - 2));

	for (int color = RED; color <= BLUE; color++) {
		// add peg configuration
		addPegPlane(color, values);

		// add link configuration
		for (int dir = 0; dir < (kNumLinkDirections / 2); dir++) {
			addLinkPlane(color, dir, values);
		}
	}

	// add plane that indicates who's turn it is
	addBinaryPlane(color, values);

}

// Adds a binary plane to the information state vector - all ones or all zeros
void Board::addBinaryPlane(int color, std::vector<double> *values) const {
	double normalizedVal = static_cast<double>(color);
	values->insert(values->end(), getSize() * (getSize() - 2), normalizedVal);
}

// Adds a plane to the information state vector corresponding to the presence
// and absence of the given link at each cell
void Board::addLinkPlane(int color, int dir, std::vector<double> *values) const {

	if (color == RED) {
		for (int y = 0 + kMargin; y < getSize() + kMargin; y++) {
			for (int x = 1 + kMargin; x < getSize() - 1 + kMargin; x++) {
				if (getConstCell({x,y})->getPeg() == color &&
					getConstCell({x,y})->hasLink(dir) )  {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	} else {
		/* BLUE => turn board 90° clockwise */
		for (int y = getSize() - 2 + kMargin; y > 0 + kMargin; y--) {
			for (int x = 0 + kMargin; x < getSize() + kMargin; x++) {
				if (getConstCell({x,y})->getPeg() == color
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
void Board::addPegPlane(int color, std::vector<double> *values) const {

	if (color == RED) {
		for (int y = 0 + kMargin; y < getSize() + kMargin; y++) {
			for (int x = 1 + kMargin; x < getSize() - 1 + kMargin; x++) {
				if (getConstCell({x,y})->getPeg() == color
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
				if (getConstCell({x,y})->getPeg() == color
						&& ! getConstCell({x,y})->hasLinks() ) {
					values->push_back(1.0);
				} else {
					values->push_back(0.0);
				}
			}
		}
	}

}


void Board::setMoveOne(Action action) {
	mMoveOne = action;
}

Action Board::getMoveOne() const {
	return mMoveOne;
};


void Board::incMoveCounter() {
	mMoveCounter++;
}

int Board::getMoveCounter() const {
	return mMoveCounter;
}

int Board::getOutlook() const {
	return mOutlook;
}

void Board::setOutlook(int outlook) {
	mOutlook = outlook;
}

void Board::setSwapped(bool swapped) {
	mSwapped = swapped;
}

bool Board::getSwapped() const {
	return mSwapped;
}

void Board::appendLinkChar(string *s, Coordinates c, enum LinkDirections dir,
		string linkChar) const {
	if (getConstCell(c)->hasLink(dir)) {
		if (getConstCell(c)->getPeg() == RED) {
			appendColorString(s, kAnsiRed, linkChar);
		} else if (getConstCell(c)->getPeg() == BLUE) {
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
	if (getConstCell(c)->getPeg() == RED) {
		// X
		appendColorString(s, kAnsiRed, "X");
	} else if (getConstCell(c)->getPeg() == BLUE) {
		// O
		appendColorString(s, kAnsiBlue, "O");
	} else if (isOnMargin(c, getSize())) {
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
	appendLinkChar(s, getNeighbour(c, {-1, 0}), LinkDirections::ENE, "/");
	appendLinkChar(s, getNeighbour(c, {-1,-1}), LinkDirections::NNE, "/");
	appendLinkChar(s, getNeighbour(c, { 0, 0}), LinkDirections::WNW, "_");
	if (len == s->length())
		s->append(" ");

	//  0, +1
	len = s->length();
	appendLinkChar(s, c, LinkDirections::NNE, "|");
	if (len == s->length())
		appendLinkChar(s, c, LinkDirections::NNW, "|");
	if (len == s->length())
		s->append(" ");

	// +1, +1
	len = s->length();
	appendLinkChar(s, getNeighbour(c, {+1, 0}), LinkDirections::WNW, "\\");
	appendLinkChar(s, getNeighbour(c, {+1,-1}), LinkDirections::NNW, "\\");
	appendLinkChar(s, getNeighbour(c, { 0, 0}), LinkDirections::ENE, "_");
	if (len == s->length())
		s->append(" ");

}

void Board::appendPegRow(string *s, Coordinates c) const {

	// -1, 0
	int len = s->length();
	appendLinkChar(s, getNeighbour(c, {-1,-1}), LinkDirections::NNE, "|");
	appendLinkChar(s, getNeighbour(c, { 0, 0}), LinkDirections::WSW, "_");
	if (len == s->length())
		s->append(" ");

	//  0,  0
	appendPegChar(s, c);

	// +1, 0
	len = s->length();
	appendLinkChar(s, c + (Coordinates) {+1,-1}, LinkDirections::NNW, "|");
	appendLinkChar(s, c + (Coordinates) { 0, 0}, LinkDirections::ESE, "_");
	if (len == s->length())
		s->append(" ");

}

void Board::appendAfterRow(string *s, Coordinates c) const {

	// -1, -1
	int len = s->length();
	appendLinkChar(s, getNeighbour(c, {+1, -1}), LinkDirections::WNW, "\\");
	appendLinkChar(s, getNeighbour(c, { 0, -1}), LinkDirections::NNW, "\\");
	if (len == s->length())
		s->append(" ");

	//  0, -1
	len = s->length();
	appendLinkChar(s, getNeighbour(c, {-1, -1}), LinkDirections::ENE, "_");
	appendLinkChar(s, getNeighbour(c, {+1, -1}), LinkDirections::WNW, "_");
	appendLinkChar(s, c, LinkDirections::SSW, "|");
	if (len == s->length()) {
		appendLinkChar(s, c, LinkDirections::SSE, "|");
	}
	if (len == s->length())
		s->append(" ");

	// -1, -1
	len = s->length();
	appendLinkChar(s, getNeighbour(c, {-1, -1}), LinkDirections::ENE, "/");
	appendLinkChar(s, getNeighbour(c, { 0, -1}), LinkDirections::NNE, "/");
	if (len == s->length())
		s->append(" ");
}

void Board::applyAction(int color, Action move) {

	// 25 26 15  9  3 21  7 29 13 16 14 28 34 17 20 22 31
	/*
	 if (mMoveCounter == 0)
	 move = 25;
	 if (mMoveCounter == 1)
	 move = 26;
	 if (mMoveCounter == 2)
	 move = 15;
	 if (mMoveCounter == 3)
	 move =  9;
	 if (mMoveCounter == 4)
	 move =  3;
	 if (mMoveCounter == 5)
	 move = 21;
	 if (mMoveCounter == 6)
	 move =  7;
	 if (mMoveCounter == 7)
	 move = 29;
	 if (mMoveCounter == 8)
	 move = 13;
	 if (mMoveCounter == 9)
	 move = 16;
	 if (mMoveCounter == 10)
	 move = 14;
	 if (mMoveCounter == 11)
	 move = 28;
	 if (mMoveCounter == 12)
	 move = 34;
	 if (mMoveCounter == 13)
	 move = 17;
	 if (mMoveCounter == 14)
	 move = 20;
	 if (mMoveCounter == 15)
	 move = 22;
	 if (mMoveCounter == 16)
	 move = 31;
	*/


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
			removeLegalAction(RED, getMoveOne());
			removeLegalAction(BLUE, getMoveOne());
		}
	}

	// calculate virtual coords from move
	Coordinates c = {
			(int) move % getSize() + kMargin,
			(int) move / getSize() + kMargin
	};

	// set peg on regular board
	getCell(c)->setPeg(color);

	// if peg is on borderline, add it to respective peg list
	int compare = color == RED ? c.second : c.first;
	if (compare - kMargin == 0) {
		addToPegSet(color, Border::START, c);
	} else if (compare - kMargin == getSize() - 1) {
		addToPegSet(color, Border::END, c);
	}

	// set links
	setSurroundingLinks(color, c);

	incMoveCounter();

	if (getMoveCounter() == 1) {
		// do not remove the move from legal actions but store it
		// because player 2 might want to swap
		setMoveOne(move);
	} else {
		// remove move one from mLegalActions
		removeLegalAction(RED, move);
		removeLegalAction(BLUE, move);
	}

	// Update the predicted result and update mCurrentPlayer...
	updateOutlook(color, c);

}

void Board::setSurroundingLinks(int color, Coordinates c) {

	bool linkedToNeutral = false;
	bool linkedToStart = false;
	bool linkedToEnd = false;

	for (int dir = 0; dir < kNumLinkDirections; dir++) {
		LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
		Coordinates cd = ld->offsets;

		if (getCell(c + cd)->getPeg() == getCell(c)->getPeg() &&
		    ! getCell(c)->isBlocked(dir) ) {
			// target peg has same color and link is not blocked
			// set link from/to this peg
			getCell(c)->setLink(dir);
			getCell(c + cd)->setLink(getOppDir(dir));

			// set blockers and check if we have
			setBlockers(c, ld);

			// check if peg we link to is in a pegset (START / END)
			if (isInPegSet(color, Border::START, c + cd)) {
				addToPegSet(color, Border::START, c);
				linkedToStart = true;
			}
			if (isInPegSet(color, Border::END, c + cd)) {
				addToPegSet(color, Border::END, c);
				linkedToEnd = true;
			}
			if (! isInPegSet(color, Border::START, c + cd) &&
				! isInPegSet(color, Border::END, c + cd)) {
				linkedToNeutral = true;
			}

		}
	}

	//check if we need to explore further
	if (isInPegSet(color, Border::START, c) && linkedToNeutral) {

		// case: new peg is in PegSet START and linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet START
		exploreLocalGraph(color, c, Border::START);
	}
	if (isInPegSet(color, Border::END, c) && linkedToNeutral) {
		// case: new peg is in PegSet END and is linked to neutral pegs
		// => explore neutral graph and add all its pegs to PegSet END
		exploreLocalGraph(color, c, Border::END);
	}

}

void Board::exploreLocalGraph(int color, Coordinates c, enum Border border) {

	// check all linked neighbors
	for (int dir = 0; dir < kNumLinkDirections; dir++) {
		if (getCell(c)->hasLink(dir)) {
			LinkDescriptor *ld = &(kLinkDescriptorTable[dir]);
			Coordinates cd = ld->offsets;
			if (! isInPegSet(color, border, c + cd)) {
				// linked neighbor is NOT yet member of PegSet
				// => add it and explore
				addToPegSet(color, border, c + cd);
				exploreLocalGraph(color, c + cd, border);
			}
		}
	}
}

}  // namespace twixt
}  // namespace open_spiel
