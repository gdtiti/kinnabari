Use "anim\_base" scene to export the character model.
Open Python shell (Alt+Shift+P) and execute [gen\_csph.py](http://code.google.com/p/kinnabari/source/browse/trunk/tool/hou/gen_csph.py) script.
This will generate culling spheres attached to joints.

![http://kinnabari.googlecode.com/svn/wiki/exp00.jpg](http://kinnabari.googlecode.com/svn/wiki/exp00.jpg)

Exporter will read the culling data from the geometry created at this step, it will not generate this data automatically.
These spheres are used at run-time to calculate a bounding box for each primitive group, then this box is used for view-frustum culling.
For actual implementation see `MDL_cull` function in [model.c](http://code.google.com/p/kinnabari/source/browse/trunk/src/model.c).

In Houdini spheres are attached to joints via Object Merge nodes (named lnk`_*`), so you can see how they move with animation.

The boxes can be visualized in Houdini as well, the script doesn't do it automatically, the following picture shows how this can be done manually:

![http://kinnabari.googlecode.com/svn/wiki/exp01.jpg](http://kinnabari.googlecode.com/svn/wiki/exp01.jpg)

To export the model use [exp\_omd.py](http://code.google.com/p/kinnabari/source/browse/trunk/tool/hou/exp_omd.py) script.
By default it will save the binary model file in your home directory, to change this set `g_omdPath` variable before executing the script:

![http://kinnabari.googlecode.com/svn/wiki/exp02.jpg](http://kinnabari.googlecode.com/svn/wiki/exp02.jpg)