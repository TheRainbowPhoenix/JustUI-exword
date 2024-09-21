from dataclasses import dataclass
from typing import Any, Tuple
import enum
import sys
import re

import juic.datatypes

@dataclass
class Loc:
    path: str
    line: int
    column: int

    def __str__(self):
        path = self.path or "(anonymous)"
        return f"{path}:{self.line}:{self.column}"

@dataclass
class SyntaxError(Exception):
    loc: Loc
    message: str

@dataclass
class Token:
    type: Any
    value: Any
    loc: Loc

    def __str__(self):
        if self.value is None:
            return f"{self.type}"
        else:
            return f"{self.type}({self.value})"

class NaiveRegexLexer:
    """
    Base class for a very naive regex-based lexer. This class provides the
    naive matching algorithm that applies all regexes at the current point and
    constructs a token with the longest, earliest match in the list. Regular
    expressions for tokens as specified in a class-wide TOKEN_REGEX list which
    consist of triples (regex, token type, token value).
    """

    # Override with list of (regex, token type, token value). Both the token
    # type and value can be functions, in which case they'll be called with the
    # match object as parameter.
    Rule = Tuple[str, Any, Any] | Tuple[str, Any, Any, int]
    TOKEN_REGEX: list[Rule] = []
    # Override with token predicate that matches token to be discarded and not
    # sent to the parser (typically, whitespace and comments).
    TOKEN_DISCARD = lambda _: False

    def __init__(self, input, inputFilename):
        self.input = input
        self.inputFilename = inputFilename
        self.line = 1
        self.column = 1

        # TODO: Precompile the regular expressions

    def loc(self):
        return Loc(self.inputFilename, self.line, self.column)

    def raiseError(self, message):
        raise SyntaxError(self.loc, message)

    def advancePosition(self, lexeme):
        for c in lexeme:
            if c == "\n":
                self.line += 1
                self.column = 0
            self.column += 1

    def nextToken(self):
        """Return the next token in the input stream, None at EOF."""
        if not len(self.input):
            return None

        highestPriority = 0
        longestMatch = None
        longestMatchIndex = -1

        for i, (regex, _, _, *rest) in enumerate(self.TOKEN_REGEX):
            priority = rest[0] if len(rest) else 0

            if (m := re.match(regex, self.input)):
                score = (priority, len(m[0]))
                if longestMatch is None or \
                    score > (highestPriority, len(longestMatch[0])):
                    highestPriority = priority
                    longestMatch = m
                    longestMatchIndex = i

        if longestMatch is None:
            nextWord = self.input.split(None, 1)[0]
            self.raiseError(f"unknown syntax '{nextWord}'")

        # Build the token
        _, type_info, value_info, *rest = self.TOKEN_REGEX[longestMatchIndex]
        m = longestMatch

        typ = type_info(m) if callable(type_info) else type_info
        value = value_info(m) if callable(value_info) else value_info
        t = Token(typ, value, self.loc())

        self.advancePosition(m[0])
        # Urgh. I need to find how to match a regex at a specific offset.
        self.input = self.input[len(m[0]):]
        return t

    def lex(self):
        """Return the next token that's visible to the parser, None at EOF."""
        t = self.nextToken()
        discard = type(self).TOKEN_DISCARD
        while t is not None and discard(t):
            t = self.nextToken()
        return t

    def dump(self, showDiscarded=False, fp=sys.stdout):
        """Dump all remaining tokens on a stream, for debugging."""
        t = 0
        discard = type(self).TOKEN_DISCARD
        while t is not None:
            t = self.nextToken()
            if t is not None and discard(t):
                if showDiscarded:
                    print(t, "(discarded)")
            else:
                print(t)

class LL1Parser:
    """
    Base class for an LL(1) recursive descent parser. This class provides the
    base mechanisms for hooking up a lexer, consuming tokens, checking the
    lookahead, and combinators for writing common types of rules such as
    expressions with operator precedence.
    """

    def __init__(self, lexer):
        self.lexer = lexer
        self.la = None
        self.advance()

    def advance(self):
        """Return the next token and update the lookahead."""
        t, self.la = self.la, self.lexer.lex()
        return t

    def atEnd(self):
        return self.la is None

    def raiseErrorAt(self, token, message):
        raise SyntaxError(token.loc, message)

    def expect(self, types, pred=None, optional=False):
        """
        Read the next token, ensuring it is one of the specified types; if
        `pred` is specified, also tests the predicate. If `optional` is set,
        returns None in case of mismatch rather than raising an error.
        """

        if not isinstance(types, list):
            types = [types]
        if self.la is not None and self.la.type in types and \
            (pred is None or pred(self.la)):
            return self.advance()
        if optional:
            return None

        expected = ", ".join(str(t) for t in types)
        err = f"expected one of {expected}, got {self.la}"
        if pred is not None:
            err += " (with predicate)"
        self.raiseErrorAt(self.la, err)

    # Rule combinators implementing unary and binary operators with precedence

    @staticmethod
    def binaryOpsLeft(ctor, ops):
        def decorate(f):
            def symbol(self):
                e = f(self)
                while (op := self.expect(ops, optional=True)) is not None:
                    e = ctor(op, [e, f(self)])
                return e
            return symbol
        return decorate

    @staticmethod
    def binaryOps(ctor, ops, *, rassoc=False):
        def decorate(f):
            def symbol(self):
                lhs = f(self)
                if (op := self.expect(ops, optional=True)) is not None:
                    rhs = symbol(self) if rassoc else f(self)
                    return ctor(op, [lhs, rhs])
                else:
                    return lhs
            return symbol
        return decorate

    @staticmethod
    def binaryOpsRight(ctor, ops):
        return LL1Parser.binaryOps(ctor, ops, rassoc=True)

    @staticmethod
    def unaryOps(ctor, ops, assoc=True):
        def decorate(f):
            def symbol(self):
                if (op := self.expect(ops, optional=True)) is not None:
                    arg = symbol(self) if assoc else f(self)
                    return ctor(op, [arg])
                else:
                    return f(self)
            return symbol
        return decorate

#---

def unescape(s: str) -> str:
    return s.encode("raw_unicode_escape").decode("unicode_escape")

class JuiLexer(NaiveRegexLexer):
    T = enum.Enum("T",
        ["WS", "KW", "COMMENT", "UNIT_TEST_MARKER",
         "TEXTLIT", "INT", "FLOAT", "STRING",
         "IDENT", "ATTR", "VAR", "LABEL", "FIELD", "CXXIDENT"])

    RE_UTMARKER = r'//\^'

    RE_COMMENT  = r'(#|//)[^\n]*|/\*([^/]|/[^*])+\*/'
    RE_INT      = r'0|[1-9][0-9]*|0b[0-1]+|0o[0-7]+|0[xX][0-9a-fA-F]+'
    # RE_FLOAT    = r'([0-9]*\.[0-9]+|[0-9]+\.[0-9]*|[0-9]+)([eE][+-]?{INT})?f?'

    RE_KW       = r'\b(else|fun|if|let|rec|set|this|null|true|false|int|bool|float|str)\b'
    RE_IDENT    = r'[\w_][\w0-9_]*'
    RE_ATTR     = r'({})\s*(?:@({}))?\s*:'.format(RE_IDENT, RE_IDENT)
    RE_VAR      = r'\$(\.)?' + RE_IDENT
    RE_LABEL    = r'@' + RE_IDENT
    RE_FIELD    = r'\.' + RE_IDENT
    RE_CXXIDENT = r'&(::)?[a-zA-Z_]((::)?[a-zA-Z0-9_])*'

    RE_STRING   = r'["]((?:[^\\"]|\\"|\\n|\\t|\\\\)*)["]'
    RE_PUNCT    = r'\.\.\.|[.,:;=(){}]'
    # TODO: Extend operator language to allow custom operators?
    RE_OP       = r'<\||\|>|->|>=|<=|!=|==|\|\||&&|<{|[|+*/%-<>!]'

    TOKEN_REGEX = [
        (r'[ \t\n]+', T.WS, None),
        (RE_COMMENT, T.COMMENT, None),
        (RE_INT, T.INT, lambda m: int(m[0], 0)),
        # FLOAT

        (RE_KW, T.KW, lambda m: m[0]),
        (RE_IDENT, T.IDENT, lambda m: m[0]),
        (RE_ATTR, T.ATTR, lambda m: (m[1], m[2])),
        (RE_VAR, T.VAR, lambda m: m[0][1:]),
        (RE_LABEL, T.LABEL, lambda m: m[0][1:]),
        (RE_FIELD, T.FIELD, lambda m: m[0][1:]),
        (RE_CXXIDENT, T.CXXIDENT, lambda m: m[0][1:]),

        (RE_STRING, T.STRING, lambda m: unescape(m[1])),
        (RE_PUNCT, lambda m: m[0], None),
        (RE_OP, lambda m: m[0], None),
    ]
    TOKEN_DISCARD = lambda t: t.type in [JuiLexer.T.WS, JuiLexer.T.COMMENT]

    def __init__(self, input, inputFilename, *, keepUnitTests):
        if keepUnitTests:
            unit_rule = (self.RE_UTMARKER, JuiLexer.T.UNIT_TEST_MARKER, None, 1)
            self.TOKEN_REGEX.insert(0, unit_rule)

        super().__init__(input, inputFilename)

@dataclass
class Node:
    T = enum.Enum("T", [
        "LIT", "IDENT", "OP", "THIS", "PROJ", "CALL", "IF", "SCOPE",
        "BASE_TYPE", "FUN_TYPE",
        "RECORD", "REC_ATTR", "REC_VALUE",
        "LET_DECL", "FUN_DECL", "REC_DECL", "SET_STMT",
        "SCOPE_EXPR", "UNIT_TEST"])
    ctor: T
    args: list[Any]

    def dump(self, indent=0):
        print("  " * indent + self.ctor.name, end=" ")
        match self.ctor, self.args:
            case Node.T.LIT, [v]:
                print(repr(v))
            case Node.T.IDENT, [v]:
                print(v)
            case Node.T.OP, [op, *args]:
                print(op)
                self.dumpArgs(args, indent)
            case _, args:
                print("")
                self.dumpArgs(args, indent)

    def dumpArgs(self, args, indent=0):
        for arg in args:
            if isinstance(arg, Node):
                arg.dump(indent + 1)
            else:
                print("  " * (indent + 1) + str(arg))

    def __str__(self):
        match self.ctor, self.args:
            case Node.T.LIT, [v]:
                return repr(v)
            case Node.T.IDENT, [v]:
                return v
            case ctor, args:
                return f"{ctor.name}({', '.join(str(a) for a in args)})"

def mkOpNode(op, args):
    return Node(Node.T.OP, [op.type] + args)

# TODO: Parser: Track locations when building up AST nodes
class JuiParser(LL1Parser):

    def expectKeyword(self, *args):
        return self.expect(JuiLexer.T.KW, pred=lambda t: t.value in args).value

    # A list of elementFunction separated by sep, with an optional final sep.
    # There must be a distinguishable termination marker "term" in order to
    # detemrine whether there are more elements incoming. "term" can either be
    # a token type or a callable applied to self.la.
    def separatedList(self, elementFunction, *, sep, term):
        elements = []
        termFunction = term if callable(term) else lambda la: la.type == term
        while not termFunction(self.la):
            elements.append(elementFunction())
            if termFunction(self.la):
                break
            self.expect(sep)
        return elements

    # expr0 ::= "null" | "true" | "false"         (constants)
    #         | INT | FLOAT | STRING | CXXIDENT   (literals)
    #         | IDENT
    #         | "(" expr ")"
    #         | "{" scope_stmt,* "}"
    def expr0(self):
        T = JuiLexer.T
        lit_kws = ["this", "null", "true", "false"]
        t = self.expect(
            [T.INT, T.FLOAT, T.STRING, T.IDENT, T.CXXIDENT, T.KW, "("],
            pred = lambda t: t.type != T.KW or t.value in lit_kws)

        match t.type:
            case T.INT | T.FLOAT | T.STRING:
                node = Node(Node.T.LIT, [t.value])
            case T.CXXIDENT:
                node = Node(Node.T.LIT, [juic.datatypes.CXXQualid(t.value)])
            case T.IDENT:
                node = Node(Node.T.IDENT, [t.value])
            case T.KW if t.value == "this":
                node = Node(Node.T.THIS, [])
            case T.KW if t.value == "null":
                node = Node(Node.T.LIT, [None])
            case T.KW if t.value in ["true", "false"]:
                node = Node(Node.T.LIT, [t.value == "true"])
            case "(":
                node = self.expr()
                self.expect(")")
        return node

    # The following are in loose -> tight precedence order:
    # expr1 ::= expr1 <binary operator> expr1
    #         | <unary operator> expr1
    #         | expr0 "{" record_entry,* "}"   (record construction)
    #         | expr0 "<{" record_entry,* "}"  (record update)
    #         | expr0 "(" expr,* ")"           (function call)
    #         | expr0 "." ident                (projection, same prec as call)
    @LL1Parser.binaryOpsLeft(mkOpNode, ["|","|>"])
    @LL1Parser.binaryOpsLeft(mkOpNode, ["<|"])
    @LL1Parser.binaryOpsLeft(mkOpNode, ["||"])
    @LL1Parser.binaryOpsLeft(mkOpNode, ["&&"])
    @LL1Parser.binaryOps(mkOpNode, [">", ">=", "<", "<=", "==", "!="])
    @LL1Parser.binaryOpsLeft(mkOpNode, ["+", "-"])
    @LL1Parser.binaryOpsLeft(mkOpNode, ["*", "/", "%"])
    @LL1Parser.unaryOps(mkOpNode, ["!", "+", "-", "..."])
    def expr1(self):
        node = self.expr0()

        # Tight postfix operators
        while (t := self.expect([JuiLexer.T.FIELD, "("], optional=True)) \
              is not None:
            match t.type:
                case JuiLexer.T.FIELD:
                    node = Node(Node.T.PROJ, [node, t.value])
                case "(":
                    args = self.separatedList(self.expr, sep=",", term=")")
                    self.expect(")")
                    node = Node(Node.T.CALL, [node, *args])

        # Postfix update or record creation operation
        while self.la is not None and self.la.type in ["{", "<{"]:
            entries = self.record_literal()
            node = Node(Node.T.RECORD, [node, *entries])

        return node

    # expr2 ::= expr1
    #         | "if" "(" expr ")" expr1 ("else" expr2)?
    def expr2(self):
        match self.la.type, self.la.value:
            case JuiLexer.T.KW, "if":
                self.expectKeyword("if")
                self.expect("(")
                cond = self.expr()
                self.expect(")")
                body1 = self.expr1()

                if self.la.type == JuiLexer.T.KW and self.la.value == "else":
                    self.expectKeyword("else")
                    body2 = self.expr2()
                else:
                    body2 = None

                return Node(Node.T.IF, [cond, body1, body2])
            case _, _:
                return self.expr1()

    def expr(self):
        return self.expr2()

    # type ::= int | bool | float | str
    #        | (type,*) -> type
    def type(self):
        builtin_type_kws = ["int", "bool", "float", "str"]
        t = self.expect([JuiLexer.T.KW, "("])
        if t.type == JuiLexer.T.KW:
            if t.value in builtin_type_kws:
                return Node(Node.T.BASE_TYPE, [t.value])
            else:
                self.raiseErrorAt(t, "not a type keyword")
        if t.type == "(":
            args_types = self.separatedList(self.type, sep=",", term=")")
            self.expect(")")
            self.expect("->")
            ret_type = self.type()
            return Node(Node.T.FUN_TYPE, [args_types, ret_type])

    # record_literal ::= "{" record_entry,* "}"
    # record_entry ::= LABEL? ATTR? expr
    #                | let_decl
    #                | fun_rec_decl
    #                | set_stmt
    def record_literal(self):
        # TODO: Distinguish constructor and update
        self.expect(["{", "<{"])
        entries = self.separatedList(self.record_entry, sep=";", term="}")
        self.expect("}")
        return entries

    def record_entry(self):
        T = JuiLexer.T
        label_t = self.expect(T.LABEL, optional=True)
        label = label_t.value if label_t is not None else None

        match self.la.type, self.la.value:
            case T.ATTR, _:
                t = self.expect(T.ATTR)
                e = self.expr()
                return Node(Node.T.REC_ATTR, [t.value[0], label, e])
            case T.KW, "let":
                if label is not None:
                    self.raiseErrorAt(label_t, "label not allowed with let")
                return self.let_decl()
            case T.KW, ("fun" | "rec"):
                if label is not None:
                    self.raiseErrorAt(label_t, "label not allowed with fun/rec")
                return self.fun_rec_decl()
            case T.KW, "set":
                if label is not None:
                    self.raiseErrorAt(label_t, "label not allowed with set")
                return self.set_stmt()
            case _, _:
                return Node(Node.T.REC_VALUE, [self.expr()])

    # let_decl ::= "let" ident (":" type)? "=" expr
    def let_decl(self):
        self.expectKeyword("let")
        t_ident = self.expect([JuiLexer.T.IDENT, JuiLexer.T.ATTR])
        if t_ident.type == JuiLexer.T.IDENT:
            ident = t_ident.value
            let_type = None
        if t_ident.type == JuiLexer.T.ATTR:
            ident = t_ident.value[0]
            let_type = self.type()
        self.expect("=")
        expr = self.expr()
        return Node(Node.T.LET_DECL, [ident, expr, let_type])

    # fun_rec_decl ::= ("fun" | "rec") ident "(" fun_rec_param,* ")" "=" expr
    # fun_rec_param ::= "..."? ident (":" type)?
    def fun_rec_param(self):
        variadic = self.expect("...", optional=True) is not None
        t_ident = self.expect([JuiLexer.T.IDENT, JuiLexer.T.ATTR])
        if t_ident.type == JuiLexer.T.IDENT:
            ident = t_ident.value
            arg_type = None
        if t_ident.type == JuiLexer.T.ATTR:
            ident = t_ident.value[0]
            arg_type = self.type()
        return (ident, variadic, arg_type)

    def fun_rec_decl(self):
        t = self.expectKeyword("fun", "rec")
        ident = self.expect(JuiLexer.T.IDENT).value
        self.expect("(")
        params = self.separatedList(self.fun_rec_param, sep=",", term=")")
        self.expect(")")
        if self.la.type == ":":
            self.expect(":")
            body_type = self.type()
        else:
            body_type = None
        self.expect("=")
        body = self.expr()
        return Node(Node.T.FUN_DECL if t == "fun" else Node.T.REC_DECL,
            [ident, params, body, body_type])

        # TODO: Check variadic param validity

    # set_stmt ::= "set" ident record_literal
    def set_stmt(self):
        self.expectKeyword("set")
        ident = self.expect(JuiLexer.T.IDENT)
        entries = self.record_literal()
        return Node(Node.T.SET_STMT, [ident, *entries])

    def scope(self):
        isNone = lambda t: t is None
        entries = self.separatedList(self.scope_stmt, sep=";", term=isNone)

        # Rearrange unit tests around their predecessors
        entries2 = []
        i = 0
        while i < len(entries):
            if i < len(entries) - 1 and entries[i+1].ctor == Node.T.UNIT_TEST:
                entries[i+1].args[0] = entries[i]
                entries2.append(entries[i+1])
                i += 2
            else:
                entries2.append(entries[i])
                i += 1

        return Node(Node.T.SCOPE, entries2)

    def scope_stmt(self):
        match self.la.type, self.la.value:
            case JuiLexer.T.KW, "let":
                return self.let_decl()
            case JuiLexer.T.KW, ("fun" | "rec"):
                return self.fun_rec_decl()
            case JuiLexer.T.KW, "set":
                return self.set_stmt()
            case JuiLexer.T.UNIT_TEST_MARKER, _:
                self.expect(JuiLexer.T.UNIT_TEST_MARKER)
                return Node(Node.T.UNIT_TEST, [None, self.expr()])
            case _:
                return Node(Node.T.SCOPE_EXPR, [self.expr()])
