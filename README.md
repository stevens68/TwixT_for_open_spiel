# TwixT-for-open_spiel

C++ implementation of the board game [TwixT](https://en.wikipedia.org/wiki/TwixT) for deepmind's framework [open_spiel](https://github.com/deepmind/open_spiel).

![TwixT board](https://github.com/stevens68/TwixT_for_open_spiel/blob/master/pics/12x12game.JPG "TwixT board")

## Installation

* Copy games/twixt* into the games folder of your open_spiel installation
* Edit games/CMakeLists.txt and add twixt.cc, twixt.h
* Build the targets as described [here](https://github.com/deepmind/open_spiel/blob/master/docs/install.md)

## Examples

    examples/example --game=twixt
    
    examples/mcts_example --game=twixt -player1=mcts --player2=mcts --max_simulations=20000 --rollout_count=4 --verbose=true
    
    python examples/examply.py --game=twixt\(board_size=12,ansi_color_output=false,discount=0.999\)


* board_size must be in [5..24], default=8
* discount must be in [0.0..1.0], default=1.0
* ansi_color_output must be true|false, default true



## Rules
* this is a paper-and-pencil variant of TwixT without link removal and without crossing of own links. 
* player 1 (X, red) plays from top to bottom, player 2 (O, blue) plays from left to right.
* If a player has no more legal moves left - when it's his turn - the game is a draw.
* The pie rule (a.k.a. swap rule) is implemented like this: 
  * player 2 decides to swap by choosing the same square as player 1.
  * the red peg is removed and a blue peg is positioned 90° clockwise instead.
  * Note: if player 1 puts his first peg on the border line, player 2 cannot swap.
  
## InformationStateTensor
* for each player we ignore the opponent's boarder lines, so the size of a plane is board_size x (board_size-2).
* for each player there are 5 planes: 1 plane for the unlinked pegs and 4 planes for the links pointing to eastern directions (North-North-East, East-North-East, East-South-East, South-South-East). See example below. We consider the unlinked pegs only, because the linked ones can be derived from the other four planes.
* Plane 11 encodes the player to move next: all 0 for player 1, all 1 for player 2.
* The state vector has shape {11, board_size, board_size-2} 

<p align="center">
<img src="https://github.com/stevens68/TwixT_for_open_spiel/blob/master/pics/animation_classic.gif" alt="6x6 game animation" width="700">
</p>
