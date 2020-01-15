# TwixT_for_open_spiel

C++ implementation of the board game [TwixT](https://en.wikipedia.org/wiki/TwixT) for deepmind's framework [open_spiel](https://github.com/deepmind/open_spiel).

![TwixT board](https://github.com/stevens68/TwixT_for_open_spiel/blob/master/pics/12x12game.JPG "TwixT board")

## Installation

* Clone deepmind's open_spiel [repository](https://github.com/deepmind/open_spiel) and follow the installation instructions
* Copy the games/twixt* files into the games folder of your open_spiel installation
* Edit games/CMakeLists.txt and add twixt.cc, twixt.h, twixt/twixtboard.cc, twixt/twixtboard.h, twixt/twixtcell.h
* Build the targets as described [here](https://github.com/deepmind/open_spiel/blob/master/docs/install.md)

## Examples

    examples/example --game=twixt
    
    examples/mcts_example --game=twixt -player1=mcts --player2=mcts --max_simulations=20000 --rollout_count=4 --verbose=true
    
    python examples/example.py --game=twixt\(board_size=12,ansi_color_output=false,discount=0.999\)


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
   
