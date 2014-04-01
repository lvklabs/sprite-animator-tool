LVK Sprite Animator Tool
========================

A WYSIWYG 2D Sprite animation tool for Cocos2d-iphone and Cocos2d-x. This tool was originally 
written to animate a complex character named "Julia" in our game "Julia A Pick Up Adventure". 
It supports Linux, Windows and Mac

*Despite this tool is outated and we hope that can be useful for someone else*


### Compile

Dependencies: Qt >= 4.5.2 

To compile, just:

    cd src
    qmake
    make

(There are some precompiled versions in the 'releases' folder)


### Screenshots


![!Tab to define frames](http://www.lvklabs.com/wp-content/uploads/2014/04/lvk-sprite-editor-frames-tab.png)
*Tab to define frames*


![!Tab to define animations](http://www.lvklabs.com/wp-content/uploads/2014/04/lvk-sprite-editor-animations-tab.png)
*Tab to define animations*


![!Tab to test transitions between animations](http://www.lvklabs.com/wp-content/uploads/2014/04/lvk-sprite-editor-transitions-tab.png)
*Tab to test transitions between animations*

### How to use

1. Create an sprite file (or open an example from the 'examples' folder)
2. Create/edit your animations
3. Export with menu option: File -> Export
4. Include the generated files .h, .lkot and .lkob in your game
5. Include the [LvkSprite-iphone](https://github.com/lvklabs/lvksprite-iphone) or [LvkSprite-x](https://github.com/lvklabs/lvksprite-x) library and load the files.

More detailed info soon. If you are interested, please send an email to contact@lvklabs.com
