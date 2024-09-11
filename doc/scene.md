# JustUI: Scenes, events and keyboard focus

## Introduction

The _scene_ in JustUI is the component sitting at the root of the widget hierarchy. It handles all the dynamic aspects of the UI, including distributing/propagating events and managing keyboard focus among the widgets.

Ideas...

New keyboard focus system.
1. Principle and conforming states
   a. Widgets have a focus policy of FOCUS_ACCEPT, FOCUS_SCOPE or FOCUS_REJECT,
      and two status flags FOCUSED and ACTIVE_FOCUSED whose valid combinations
      describe 3 focus levels: *no focus* (F=0), *inactive focus* (F=1, AF=0),
      and *active focus* (F=1, AF=1).
   b. Each focus scope defines a region consisting of its subtree but excluding
      itself and any children of other focus scopes. Up to one widget of policy
      FOCUS_ACCEPT or FOCUS_SCOPE in the region may have its FOCUSED flag set,
      called the *focus target* of the scope. The ACTIVE_FOCUSED flag of the
      target is equal to the scope's.
   c. Every focus scope fs induces a *focus chain* fc(fs), where
        fc(w) = [] if w is FOCUS_ACCEPT or w is FOCUS_SCOPE with no target;
        fc(w) = fc(target) + [target] if w is a scope with the named target.
      Note that as a result of 1.b all elements of the chain have their FOCUSED
      flag set and their ACTIVE_FOCUSED flags equal to that of fs.
   d. jscene is a focus scope defined to have its ACTIVE_FOCUSED always set,
      the only such widget that doesn't inherit this flag from a scope parent.
      As such, the ACTIVE_FOCUSED flag identifies the focus chain of the scene.
      jscene offers keyboard events to its focus chain, propagating rejected
      events in list order.
   X. The first (deepest) widget in the scene's focus chain is said to have
      *strong focus*. Other widgets this chain are said to have *weak focus*.
      -> TODO: Strong/weak focus is an informal substate of active focus and
         may not be kept in the future.
2. Operations
   a. The core operation is a changing the target of a scope, which can be
      initiated by any widget in the region through a widget-context call. The
      consequences of this operation are as follow:
      - The old target, if any, loses its FOCUSED and ACTIVE_FOCUSED flags. If
        it's a scope and it had active focus, all widgets in its focus chain
        also lose active focus.
      - The new target, if any, gets its FOCUSED flag and the scope's value for
        the ACTIVE_FOCUSED flag. If it's a focus scope and it gets active
        focus, then its focus chain also gets active focus.
      All widgets affected receive a FOCUS_CHANGED event and the scope itself
      receives a FOCUS_TARGET_CHANGED event.
   b. There is a widget-scene call to give a widget active focus. If the widget
      has policy FOCUS_ACCEPT or FOCUS_SCOPE, this call walks up the scope
      chain and assigns new targets until it reaches the scene. If the widget
      has policy FOCUS_NONE, this call just removes the surrounding scope's
      target.
   X. There is a widget-context call to relinquish one's own focus within the
      parent scope. This is intended to differ from 2.b in that scopes might
      implement special logic for moving focus to a nearby widget.
      -> TODO: Left unresolved for later
3. Key-listener pattern
   a. The operations described in 2.a and 2.b allow a "key-listener" pattern
      where a widget "klw" (not the scene) receives keyboard events from its
      potentially-focused descendants (i.e. inserts itself in the focus chain)
      without impacting the focus mechanics of its surrounding scope "ssw".
   b. The pattern consists of making klw a focus scope, and whenever it
      receives a FOCUS_TARGET_CHANGED event that assigns a target, make klw
      grab focus within ssw. This way, with regards to ssw, when focus moves:
      - From within klw to within klw:
          the grab is a no-op: OK.
      - From within klw to outside it:
          outside widget gets focus within ssw and klw does nothing: OK.
      - From outside klw to within it:
          inside widget gets focus in klw, klw gets focus in ssw: OK.
      - From outside klw to outside it:
          normal retargeting within ssw.
   X. Should this be a built-in behavior?

4. Behavior of built-in widgets
   a. `jfileselect`, `jinput`, `jlist` have policy FOCUS_ACCEPT
   b. `jscene` has policy FOCUS_SCOPE
      TODO: What about jframe?
   c. `jfkeys` has policy `FOCUS_REJECT` and must be given events manually
   d. Widgets with a stack layout will try and give focus (within their
      surrounding scope) to the current element. If the element is complex, it
      should be a focus scope. Similarly, widgets with invisible children will
      move focus around to their direct children as needed to make sure focus
      remains on a visible child.

5. Implementation
   a. Events are `FOCUS_CHANGED` when own FOCUSED and ACTIVE_FOCUSED flags have
      changed, and `FOCUS_TARGET_CHANGED` for scopes when the target changes.

NOTES:
- Focus events don't propagate but are replicated, which adds some complexity
- Focus scopes could have background focus for jfkeys (left for later). Hard
  part is this isn't specified by jfkeys.
  Could tell users to add this to their main loop, or follow gscreen:
    jevent e = jscene_run();
    if(jwidget_event(fkeys, e)) continue;
- Is there any sensible definition to "relinquishing" focus? In gintctl if e.g.
  an jinput validates we want to take its focus away and put it back somewhere
  sensible (i.e. the gscreen, not NULL, otherwise we don't have F-keys anymore)
- Some functions from <jwidget-api.h> should go to <jwidget.h>
- Functions from <jwidget.h> that require context or operate on the whole tree
  should **really** be identified clearly. Everything by default should just
  treat the widget as a box in a void.
  -> Previously this was in jscene. Why not keep it that way?

## Keyboard focus and event propagation

TODO.
