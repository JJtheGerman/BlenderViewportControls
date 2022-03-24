# BlenderViewportControls
Plugin that ports some of Blenders most useful viewport controls to the Unreal Editor.

## Currently work in progress!

### Features

- After installing the plugin you will have a new tool mode accessible in the top left of the editor
  - By default this is Shift+6 if you do not have any extra tools installed.

#### Press G to enter move mode
![1](Resources/MoveMode.gif)  
- Press (Shift) + X,Y,Z to Axis/Dual Axis Lock
- Double press X,Y,Z to toggle between worldspace and localspace axes  
- Press Shift to use Precision Mode  
- Press Ctrl to snap selection to surface and align rotation with normal. *(Use scroll wheel to change offset while moving)*    


#### Press R for rotate mode
![2](Resources/RotateMode.gif)  
- Double press R to enter trackball rotation  
- Press X,Y,Z to Axis Lock  
- Double press X,Y,Z to toggle between worldspace and localspace axes  
- Press Ctrl to angle snap  
- Press Shift to use Precision Mode  


#### Press S for scale mode
![3](Resources/ScaleMode.gif)  
- Press (Shift) + X,Y,Z to Axis/Dual Axis Lock (dual axis scaling is currently broken)
- Double press X,Y,Z to toggle between worldspace and localspace axes  
- Press Ctrl to increment scale  
- Press Shift to use Precision Mode   

#### Alt + G | R | S to reset transforms

#### Shift + D to duplicate

*Random note, transforming thousands of objects at once is SIGNIGICANTLY faster in this plugin than standard unreal, so if you for whatever reason need to move a thousand objects at a time, this is for you :)*
