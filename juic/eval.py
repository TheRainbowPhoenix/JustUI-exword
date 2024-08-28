from dataclasses import dataclass, field
from typing import Tuple, Union

import juic.parser
from juic.parser import Node
from juic.datatypes import *

# Storage of a scope's internal data, including all definitions in the scope.
# The data in this object is never copied, so for instance, the name -> value
# mappings are always unique and can only be found in the relevant's scope
# unique MutableScopeData instance. This data is, of course, not constant,
# which makes it unsuitable for building closures. The Closure type captures an
# immutable snapshot of a scope's contents at one point in time.
@dataclass
class MutableScopeData:
    # List of definitions. Each value has a timestamp integer that indicates
    # when it was added; this allows snapshots to ignore definitions newer than
    # the snapshot by simply comparing timestamp values. The value itself is
    # either a thunk (evaluated or not), or a runtime value.
    defs: dict[str, Tuple[int, Union[JuiValue, "Thunk"]]] = \
        field(default_factory=dict)

    # Current timestamp, changed every time a value is added.
    timestamp: int = 0

    def addDefinition(self, name: str, content: Union[JuiValue, "Thunk"]):
        if name in self.defs:
            raise JuiRuntimeError(f"{name} already defined in current scope")
        self.defs[name] = (self.timestamp, content)
        self.timestamp += 1

    def lookup(self, name: str, maxTimestamp: int = -1) \
            -> Union[JuiValue, "Thunk", None]:
        if name not in self.defs:
            return None
        if maxTimestamp >= 0 and self.defs[name][0] >= maxTimestamp:
            return None
        return self.defs[name][1]

    def dump(self):
        digits = len(str(self.timestamp))
        print(f"Scope: timestamp={self.timestamp}, defs:")
        for name, (ts, content) in self.defs.items():
            print(f"  {ts: >{digits}}  {name}:", juiValueString(content))

# An immutable snapshot of all definitions that can be referred to at a given
# point in time. This specifies the interpretation of all defined identifiers
# but not of the "this" keyword.
@dataclass
class Closure:
    # Parent snapshot on which this one is based, which defines the contents of
    # surrounding scopes at that time.
    parent: Union["Closure", None]
    # Reference to the scope in which the closure was created.
    scope: MutableScopeData
    # Timestamp at which the closure was created. Definitions from `scope`
    # created later than the closure will be ignored during name lookup.
    timestamp: int = field(init=False)

    def __post_init__(self):
        # Automatically get the timestamp at the time of creation
        self.timestamp = self.scope.timestamp

    def lookup(self, name: str) -> Union[JuiValue, "Thunk", None]:
        value = self.scope.lookup(name, self.timestamp)
        if value is not None or self.parent is None:
            return value
        return self.parent.lookup(name)

    def dump(self, level=0):
        print(f"Closure at depth {level}: timestamp={self.timestamp}:")
        self.scope.dump()
        if self.parent is not None:
            self.parent.dump(level+1)

# Thunk representing a suspended/lazy computation. Thunks are evaluated when
# needed (which is the call-by-need evaluation strategy). They are always
# interpreted in the context of the associated closure.
#
# While the object is technically mutable, the result of the computation
# (whether a value or a cyclic invalid result) is entirely determined by the
# expression AST and closure, so the result is just a lazily computed/memoized
# member. That said, the result might not exist if there is a cyclic dependency
# between this and other thunks referenced by the closure.
@dataclass
class Thunk:
    # Expression AST that the thunk evaluates to
    ast: Node
    # Closure in which the thunk can be evaluated, plus the definition for
    # "this"; if there is one, it is always a PRS
    closure: Closure
    thisReference: Union[None, "PartialRecordSnapshot"] = None

    # Whether the thunk has been evaluated yet
    evaluated: bool = False
    # Whether the thunk has been evaluated to an invalid result because of a
    # cyclic dependency with other thunks captured by the closure
    invalid: bool = False
    # If the thunk has been evaluated and is not invalid, resulting value. None
    # is a possible resulting value (`null` in the language); do not use this
    # field to determine whether the thunk has been successfully evaluted.
    result: JuiValue = None

    # Whether the thunk is currently under evaluation
    _running: bool = False

    def __str__(self):
        s = "Thunk("
        if th._running:
            s += "running, "
        if th.evaluated:
            s += "evaluated"
            if th.invalid:
                s += ", invalid"
            else:
                s += " = " + juiValueString(th.result)
        else:
            s += "unevaluated"
        s += "){ " + str(th.ast) + " }"
        return s

# Object representing the interpretation of a partially-built record. In a
# record construction or update operation, fields of `this` refer to the latest
# definition before the point of use, if there is one, or the definition in the
# underlying record otherwise, if there is one. Because the use of record
# constructor arguments can create forwards dependencies, all these definitions
# are thunked and computed lazily in a somewhat unpredictable order. A partial
# record snapshot fully specifies the interpretation of all fields of a
# partially-constructed record (i.e. `this`) at the location of one record
# entry. It does so by mapping defined field names to the correct thunks. This
# object is how dependencies between fields are discovered and followed.
@dataclass
class PartialRecordSnapshot:
    # Mapping from fields of `this` to thunks
    fieldThunks: dict[str, Thunk] = field(default_factory=dict)
    # Base object for fields not captured in `fieldThunks`
    base: None | Thunk = None

    def __str__(self):
        return "<A PRS AAAAAAH>" # TODO

    def copy(self):
        return PartialRecordSnapshot(self.fieldThunks.copy(), self.base)

class JuiTypeError(Exception):
    pass

class JuiNameError(Exception):
    pass

class JuiStaticError(Exception):
    pass

class JuiRuntimeError(Exception):
    pass

# TODO: Better diagnostics for type errors
def requireType(v: JuiValue, predicate: Callable[[JuiValue], bool]):
    if not predicate(v):
        raise JuiTypeError(f"type error: got {v}, needed {predicate}")

def requireSameType(vs: list[JuiValue], predicate: Callable[[JuiValue], bool]):
    if len(vs) == 0:
        return
    if len(set(type(v) for v in vs)) != 1:
        vals = ", ".join(str(v) for v in vs)
        raise JuiTypeError(f"type error: heterogeneous types for {vals}")

    if not predicate(vs[0]):
        raise JuiTypeError(f"type error: got {vs[0]}, needed {predicate}")

# Evaluation context. This tracks the current scope and provides evaluation
# functions, error tracking, etc.
class Context:
    # Current top-level scope. Can be None. This is the only scope in which we
    # can add definitions.
    currentScope: MutableScopeData
    # Current closure defining the scopes that surround `currentScope`.
    currentClosure: Closure | None

    def __init__(self, initialClosure = None):
        self.currentScope = MutableScopeData()
        self.currentClosure = initialClosure
        self._contextStack = []

    #=== Context switch commodity ===#

    _contextStack: list[Tuple[MutableScopeData, Closure]]

    class ContextSwitchContextManager:
        def __init__(self, parent: "Context", c: Closure):
            self.parent = parent
            self.targetClosure = c

        def __enter__(self):
            p = self.parent
            p._contextStack.append((p.currentScope, p.currentClosure))
            p.currentScope = MutableScopeData()
            p.currentClosure = self.targetClosure

        def __exit__(self, exc_type, exc_value, traceback):
            p = self.parent
            s, c = p._contextStack.pop()
            p.currentScope = s
            p.currentClosure = c

    # Temporarily switch the execution context. This is used when we need e.g.
    # to evaluate a thunk in its associated closure. Morally one could
    # construct a different Context object, but it's easier to accumulate error
    # messages and other things in the same Context object.
    # Example:
    #     with self.contextSwitchAndPushNewScope(myClosure):
    #         myResult = self.eval(myExprNode)
    def contextSwitchAndPushNewScope(self, c: Closure):
        return Context.ContextSwitchContextManager(self, c)

    # Freeze the current scope and push a new empty one to start working in.
    # Example:
    #     with self.pushNewScope():
    #         self.addDefinition(myParamName, myParamValue)
    #         myResult = self.eval(myFunctionBody)
    def pushNewScope(self):
        return self.contextSwitchAndPushNewScope(self.currentStateClosure())

    #=== State management ===#

    # Build a the closure of the current state
    def currentStateClosure(self):
        return Closure(parent=self.currentClosure, scope=self.currentScope)

    # Add a new definition in the innermost scope
    def addDefinition(self, name: str, content: JuiValue | Thunk):
        return self.currentScope.addDefinition(name, content)

    # Lookup a name
    def lookup(self, name: str) -> JuiValue | Thunk | None:
        return self.currentStateClosure().lookup(name)

    #=== Evaluation helpers ===#

    def makeFunction(self, params: list[Tuple[str, bool]], body: Node) \
            -> Function:
        # Check validity of the variadic parameter specification
        variadics = [i for i, (_, v) in enumerate(params) if v]

        for (name, _) in params:
            count = len([0 for (n, _) in params if n == name])
            if count > 1:
                raise JuiStaticError(f"duplicate argument {name}")

        if len(variadics) > 1:
            names = ", ".join(params[i][0] for i in variadics)
            raise JuiStaticError(f"multiple variadic arguments: {names}")

        if variadics and variadics != [len(params) - 1]:
            name = params[variadic][0]
            raise JuiStaticError(f"variadic argument {name} not at the end")
        variadic = params[variadics[0]][0] if variadics else None

        # Build function object
        return Function(closure = self.currentStateClosure(),
                        body = body,
                        params = [name for (name, v) in params if not v],
                        variadic = variadic)

    #=== Main evaluation functions ===#

    # Expression evaluator; this function is pure. It can cause thunks to be
    # evaluated and thus reveal previously-undiscovered cyclic dependencies,
    # but it doesn't modify the context.
    def evalExpr(self, node: Node) -> JuiValue:
        match node.ctor, node.args:
            case Node.T.LIT, [x]:
                return x

            case Node.T.IDENT, [name]:
                match self.lookup(name):
                    case None:
                        raise JuiNameError(f"name {name} not defined")
                    case Thunk() as t:
                        return self.evalThunk(t)
                    case value:
                        return value

            case Node.T.OP, ["+", X, Y]:
                x = self.evalExpr(X)
                y = self.evalExpr(Y)
                requireSameType([x, y], juiIsAddable)
                return x + y

            # TODO: Arithmetic: integer division, type conversions
            case Node.T.OP, [("+" | "-" | "*" | "/" | "%") as op, X, Y]:
                x = self.evalExpr(X)
                y = self.evalExpr(Y)
                requireSameType([x, y], juiIsArith)
                match op:
                    case "+": return x + y
                    case "-": return x - y
                    case "*": return x * y
                    case "/": return x / y if type(x) == float else x // y
                    case "%": return x % y

            case Node.T.OP, [("!" | "+" | "-") as op, X]:
                x = self.evalExpr(X)
                requireType(x, juiIsArith)
                match op:
                    case "!": return not x
                    case "+": return +x
                    case "-": return -x

            case Node.T.OP, [(">"|">="|"<"|"<="|"=="|"!=") as op, X, Y]:
                x = self.evalExpr(X)
                y = self.evalExpr(Y)
                requireSameType([x, y], juiIsComparable)
                match op:
                    case ">": return x > y
                    case "<": return x < y
                    case ">=": return x >= y
                    case "<=": return x <= y
                    case "==": return x == y
                    case "!=": return x != y

            case Node.T.OP, [("&&" | "||") as op, X, Y]:
                x = self.evalExpr(X)
                y = self.evalExpr(Y)
                requireSameType([x, y], juiIsLogical)
                match op:
                    case "&&": return x and y
                    case "||": return x or y

            case Node.T.OP, ["...", X]:
                x = self.evalExpr(X)
                requireType(x, juiIsUnpackable)
                raise NotImplementedError("unpack operator o(x_x)o")

            case Node.T.OP, ["|", X, F]:
                f = self.evalExpr(F)
                requireType(f, juiIsCallable)
                return self.evalCall(f, [X])

            case Node.T.OP, ["<|", F, X]:
                f = self.evalExpr(F)
                requireType(f, juiIsCallable)
                return self.evalCall(f, [X])

            case Node.T.THIS, []:
                raise NotImplementedError

            case Node.T.PROJ, []:
                raise NotImplementedError

            case Node.T.CALL, [F, *A]:
                f = self.evalExpr(F)
                requireType(f, juiIsCallable)
                return self.evalCall(f, A)

            case Node.T.IF, [C, T, E]:
                c = self.evalExpr(C)
                requireType(c, juiIsLogical)
                if bool(c):
                    return self.evalExpr(T)
                else:
                    return None if E is None else self.evalExpr(E)

            case Node.T.RECORD, [R, *A]:
                r = self.evalExpr(R)
                requireType(r, juiIsConstructible)
                return self.evalRecordConstructor(r, A)

            case Node.T.REC_ATTR, _:
                raise NotImplementedError

            case Node.T.REC_VALUE, _:
                raise NotImplementedError

            case _, _:
                raise Exception("invalid expr o(x_x)o: " + str(node))

    def execStmt(self, node: Node) -> JuiValue:
        match node.ctor, node.args:
            case Node.T.LET_DECL, [name, X]:
                self.addDefinition(name, self.evalExpr(X))

            case Node.T.FUN_DECL, [name, params, body]:
                self.addDefinition(name, self.makeFunction(params, body))

            case Node.T.REC_DECL, _:
                raise NotImplementedError

            case Node.T.SET_STMT, _:
                raise NotImplementedError

            case Node.T.UNIT_TEST, [subject, expected]:
                vs = self.evalExpr(subject)
                ve = self.evalExpr(expected)
                vs = self.force(vs)
                ve = self.force(ve)
                if not juiValuesEqual(vs, ve):
                    print("unit test failed:")
                    print("  " + str(vs))
                    print("vs.")
                    print("  " + str(ve))

            case _, _:
                return self.evalExpr(node)

    # TODO: Context.eval*: continue failed computations to find other errors?
    def evalThunk(self, th: Thunk) -> JuiValue:
        if th.evaluated:
            if th.invalid:
                raise JuiRuntimeError("cyclic dependency result encountered")
            return th.result
        if th._running:
            raise JuiRuntimeError("cyclic dependency detected!")

        with self.contextSwitchAndPushNewScope(th.closure):
            if th.thisReference is not None:
                self.addDefinition("this", th.thisReference)

            th._running = True
            result = self.evalExpr(th.ast)
            th._running = False

        # TODO: Set invalid = True if we continue failed computations
        th.evaluated = True
        th.result = result
        return result

    def evalCall(self, f: JuiValue, args: list[Node]) -> JuiValue:
        # Built-in functions: just evaluate arguments and go
        # TODO: Check types of built-in function calls
        if type(f) == BuiltinFunction:
            return f.func(*[self.evalExpr(a) for a in args])

        assert type(f) == Function and "evalCall: bad type check precondition"

        # Check number of arguments
        req = str(len(f.params)) + ("+" if f.variadic is not None else "")
        if len(args) < len(f.params):
            raise JuiRuntimeError(f"not enough args (need {req}, got {len(args)})")
        if len(args) > len(f.params) and f.variadic is None:
            raise JuiRuntimeError(f"too many args (need {req}, got {len(args)})")

        # TODO: In order to build variadic set I need a LIST node
        if f.variadic is not None:
            raise NotImplementedError("list node for building varargs o(x_x)o")

        # Run into the function's scope
        with self.contextSwitchAndPushNewScope(f.closure):
            for name, node in zip(f.params, args):
                th = Thunk(ast=node, closure=self.currentStateClosure())
                self.addDefinition(name, th)
            # self.currentScope.dump()
            # self.currentClosure.dump()
            return self.execStmt(f.body)

    def evalRecordConstructor(self, ctor: JuiValue, entries: list[Node]) \
            -> JuiValue:

        # Collect indices of fields and arguments separately; keep the order
        args = []
        attr = []
        for i, e in enumerate(entries):
            if e.ctor == Node.T.REC_ATTR:
                attr.append(i)
            elif e.ctor == Node.T.REC_VALUE:
                args.append(i)
            else:
                assert False and "record node has weird children o(x_x)o"

        # Base record constructor: starts out with an empty record. All
        # arguments are children. Easy.
        if type(ctor) == RecordType:
            r = Record(base=ctor, attr=dict(), children=dict())

            if len(args) > 0:
                raise JuiRuntimeError(f"arguments given to type rec ctor")

            # Create thunks for all entries while providing them with
            # progressively more complete PRS of r.
            prs = PartialRecordSnapshot()

            for i in attr:
                name, label, node = entries[i].args
                th = Thunk(ast=node, closure=self.currentStateClosure())
                th.thisReference = prs.copy()

                r.attr[name] = th
                prs.fieldThunks[name] = th

            return r

        # TODO: Factor this with function. In fact, this should reduce to a call
        # Check number of arguments
        req = str(len(ctor.params)) + ("+" if ctor.variadic is not None else "")
        if len(args) < len(ctor.params):
            raise JuiRuntimeError(f"not enough args (need {req}, got {len(args)})")
        if len(args) > len(ctor.params) and ctor.variadic is None:
            raise JuiRuntimeError(f"too many args (need {req}, got {len(args)})")

        # TODO: In order to build variadic set I need a LIST node
        if ctor.variadic is not None:
            raise NotImplementedError("list node for building varargs o(x_x)o")

        # Otherwise, it's a function so it might use
        raise NotImplementedError

        assert type(r) == Function and \
            "evalRecordConstructor: not a record type nor a function"

        # Thunks should be created from a closure of the entire environment
        # -> This environment is always known, _except_ for
        #    partially-constructed records
        # So, naturally... patch the PRS for "this" later on?
        #
        # ## In a normal function call
        #
        # Invariant: context of evaluating the expression that has the call is
        # complete. Use it for all the arguments. Done.
        #
        # ## In a record constructor with no parameters
        #
        # All attribute values have the same context when excluding "this",
        # it's just the context surrounding the record constructor, and
        # (invariant) it is complete. Use it to create all of the thunks.
        #
        # Later, iterate through the thunks in top-down order and patch the
        # "this" by constructing PRSs.
        #
        # Invariant is satisified because each thunk's closure is complete and
        # thus the  context for evaluating their contents will also be
        # complete.
        #
        # ## In a record constructor with parameters
        #
        # Again, the entire context except for "this" is well-defined and
        # complete. Use it to create thunks for all record entries, inline or
        # not.
        #
        # Prepare a thunk for the call's evaluation, based on the function's
        # environment, assigning parameter-related thunks (or lists thereof) as
        # definitions for the function's arguments.
        #
        # Use the call's thunk as base for the PRS and sweep top-to-bottom
        # assigning PRSs in every record-entry thunk.
        #
        # Return a literal record with all non-parameter entries as elements.
        # Erm, remove duplicates.
        #
        # NOTE: NO WAY TO SPECIFY AN ATTRIBUTE IN A NON-STATIC WAY.

        # - Build thunks for all entries, with one layer of "this" data
        # - Call ctor1, pass it the argument thunks
        # - Move into ctor1's scope, add new scope with arguments
        # - Build a thunk for ctor1's body, this is second layer of "this"
        # /!\ It's not per-field! It's not per-field!
        # - Go back to toplevel, add second layer of "this" to all thunks
        # - Return a PartialRecord object that has ctor1's returned thunk as
        #   base and the other fields as overrides.
        #
        # ... Looks too opaque at ctor1's level.
        # ... But then again for lazy evaluation all we need to know is it's a
        #     record, and then we evaluated only when we project fields. If the
        #     same thing happens with <{} there should be no problem.
        # ... Having exactly two layers is suspicious. What happens when we try
        #     to get a field?
        #
        # 1. Lookup the field in the PartialRecord. If it has it, it's been
        #    added by the latest constructor, and we know which it is. Evaluate
        #    the thunk inside.
        # 2. Otherwise, evaluate the base thunk. In the "f <| rec { ... }" it's
        #    a record update. This will yield another PartialRecord. Use it.
        # 3. This should work, but we need a PartialRecord.
        #
        # When do we convert back to a full Record?
        # How about that's what Record does, and it just has accessors?
        raise NotImplementedError

    def force(self, v: JuiValue) -> JuiValue:
        match v:
            case Record() as r:
                for a in r.attr:
                    r.attr[a] = self.force(r.attr[a])
                return r
            case Thunk() as th:
                self.evalThunk(th)
                if th.evaluated:
                    return self.force(th.result)
                return th
            case _:
                return v
