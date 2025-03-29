# JustUI: Scenes, events and keyboard focus

## Introduction

The _scene_ in JustUI is the component sitting at the root of the widget hierarchy. It handles all the dynamic aspects of the UI, including distributing/propagating events and managing keyboard focus among the widgets.

TODO...

## Keyboard focus system

### Principles: focus policies, focused widgets and active-focus widgets

Every widget has a focus policy, which can be:
* `J_FOCUS_POLICY_REJECT`: The widget does not receive keyboard input and is ignored for the purposes of input handling (its children are not ignored though). This is the default and is usually overridden at creation by `jwidget_set_focus_policy()`.
* `J_FOCUS_POLICY_ACCEPT`: The widget receives keyboard input and keeps it to itself, not allowing any children to get it. Children will not see any events unless the widget's event function decides to forward them.
* `J_FOCUS_POLICY_SCOPE`: The widget can receive focus but also allows a child to get focus. In this case, events that the child is not interested in will be offered to the scope widget.

Additionally, widgets have three focus states defined by two status flags `FOCUSED` and `ACTIVE_FOCUSED`:

* No focus - `FOCUSED=0`
* Inactive focus - `FOCUSED=1`, `ACTIVE_FOCUSED=0`
* Active focus - `FOCUSED=1`, `ACTIVE_FOCUSED=1`

Roughly speaking, active focus means the widget currently receives input, whereas inactive focus means the widget has focus within a parent that is inactive. For instance, an input field within a tab that is currently invisible would have inactive focus. If the tab becomes visible, the input field gets active focus. Inactive focus essentially remembers who had focus while certain parts of the interface are not being used.

Each widget w with the `J_FOCUS_POLICY_SCOPE` policy defines a region called a *focus scope*, which is essentially the widgets whose focus is managed by w. This region consists of w's subtree, excluding w itself and any children of other focus scopes.

The fact that w can transmit its focus to a child is formalized as follows. Within the scope, up to one widget with policy `J_FOCUS_POLICY_ACCEPT` or `J_FOCUS_POLICY_SCOPE` can have its `FOCUSED` flag set and this widget is called the *focus target* of the scope. The focus target has the same value for the `ACTIVE_FOCUSED` flag as w.

If the focus target is also a scope, it may itself have a target. Thus, each scope induces a *focus chain* FC, defined mathematically by

* `FC(w) = []` if w is not a scope or has no target;
* `FC(w) = FC(target) + [target]` if w is a scope with the given target.

The elements of the chain are exactly the children of w that have their `FOCUSED` flag set, and they all have the same value for `ACTIVE_FOCUSED` as w.

`jscene`, where focus originates, is a scope and is defined to have its `ACTIVE_FOCUSED` flag set at all times (the only widget that doesn't inherit this flag from a scope parent).  Thus, the `ACTIVE_FOCUSED` flag identifies the focus chain of the scene. When given an event, `jscene` offers it to all of the elements of its focus chain, starting from the deepest children and then to the parents, until one accepts the event.

TODO: Diagram! It's not straightforward.

### Operations

**Changing the target of a scope**

Focus can moved by changing the target of a scope, with a widget-context function call. When this happens:

- The old target, if any, loses its `FOCUSED` and `ACTIVE_FOCUSED` flags. If it's a scope and it had active focus, all widgets in its focus chain also lose active focus.
- The new target, if any, gets its `FOCUSED` flag and the scope's value for the `ACTIVE_FOCUSED` flag. If it's a focus scope and it gets active focus, then its focus chain also gets active focus.

All widgets affected receive a `FOCUS_CHANGED` event and the scope itself receives a `FOCUS_TARGET_CHANGED` event.

**Giving a widget active focus**

There is a scene function to give a widget active focus.
- If the widget has policy `J_FOCUS_POLICY_ACCEPT` or `J_FOCUS_POLICY_SCOPE`, this functions gives active focus to all of its sopce ancestors until it reaches the scene.
walks up the scope chain and assigns new targets until it reaches the scene.
- If the widget is `NULL`, this call just removes the surrounding scope's target.
- [TODO] If the widget has policy `J_FOCUS_POLICY__REJECT`, it clears scopes from that widget up until reaching the scene.

**Relinquishing focus**

[TODO] It is intended that there by a widget-context function to relinquish one's own focus within the parent scope. This is intended to allow scopes to implement special logic for moving focus to a nearby widget automatically.

### Key-listener pattern

The operations described above allow a "key-listener" pattern where a widget KL (not the scene) receives keyboard events from its potentially-focused descendants (i.e. inserts itself in the focus chain) without impacting the focus mechanics of its surrounding scope SS.

The pattern consists of making KL a focus scope, and whenever it receives a `FOCUS_TARGET_CHANGED` event that assigns a target, make KL grab focus within SS. This way, with regards to SS, when focus moves:
* From within KL to within KL: the grab is a no-op: OK.
* From within KL to outside it: outside widget gets focus within SS and KL does nothing: OK.
* From outside KL to within it: inside widget gets focus in KL, KL gets focus in SS: OK.
* From outside KL to outside it: KL is not involved: OK.

[TODO] Should this be a built-in behavior?

## Behavior of built-in widgets

- `jfileselect`, `jinput`, `jlist` have policy `J_FOCUS_POLICY_ACCEPT`
- `jscene`, `jscrolledlist` have policy `J_FOCUS_POLICY_SCOPE`
- `jfkeys` has policy `FOCUS_REJECT` and must be given events manually (awkward)
- [TODO] Widgets with a stack layout will try and give focus (within their surrounding scope) to the current element. If the element is complex, it should be a focus scope. Similarly, widgets with invisible children will move focus around to their direct children as needed to make sure focus remains on a visible child.
