# Just User Interfaces

JustUI is a "small" library to make GUIs on fx-9860G and fx-CG 50 with gint
(and I mean small by GUI library standards). I've built it to improve user
interfaces in [gintctl](https://gitea.planet-casio.com/Lephenixnoir/gintctl),
but it's still documented enough that you can try it if you need GUIs.

Features include:

* Widget trees with parent/children ownership
* Basic `jwidget` and derived types (`jlabel`, `jinput`, etc.)
* A good layout system to determine the size and position of widgets
* Low-effort custom widgets (`jpainted`) and completely custom widget types
* Decent keyboard focus and event system

Screenshots: TODO

Documentation:

* [Widget types](doc/widgets.md)
* [Widget hierarchy](doc/hierarchy.md)
* [Space distribution and layout](doc/layout.md)
* [Scenes and events](doc/scene.md) (TODO)
* Everything else is explained in the headers.
