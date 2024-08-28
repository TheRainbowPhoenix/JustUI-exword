import getopt
import sys

import juic.parser
import juic.eval
import juic.builtins

USAGE_STRING = """
usage: juic [OPTIONS] INPUT

JustUI high-level description compiler.

Options:
  --debug=lexer      Dump the lexer output and stop.
         =parser     Dump the parser output and stop.
""".strip()

def usage(exitcode=None):
    print(USAGE_STRING, file=sys.stderr)

    if exitcode is not None:
        sys.exit(exitcode)

def main(argv):
    # Read command-line arguments
    try:
        opts, args = getopt.gnu_getopt(argv[1:], "", ["help", "debug="])
        opts = dict(opts)

        if len(argv) == 1 or "-h" in opts or "--help" in opts:
            usage(0)
        if len(args) < 1:
            usage(1)
        if len(args) > 1:
            raise getopt.GetoptError("only one input file can be specified")

    except getopt.GetoptError as e:
        print("error:", e, file=sys.stderr)
        print("Try '{} --help' for details.".format(argv[0]), file=sys.stderr)
        sys.exit(1)

    #---

    with open(args[0], "r") as fp:
        source = fp.read()

    try:
        lexer = juic.parser.JuiLexer(source, args[0])
        if opts.get("--debug") == "lexer":
            lexer.dump()
            return 0

        parser = juic.parser.JuiParser(lexer)
        ast = parser.scope()
        if opts.get("--debug") == "parser":
            for e in ast.args:
                e.dump()
                print("---")
            return 0

    except juic.parser.SyntaxError as e:
        print(e.loc, ": ", "\x1b[31merror:\x1b[0m ", e.message, sep="",
            file=sys.stdout)
        return 1

    ctx = juic.eval.Context(juic.builtins.builtinClosure)

    for e in ast.args:
        e.dump()
        v = ctx.execStmt(e)
        v = ctx.force(v)
        print(">>>>>>>", juic.eval.juiValueString(v))

    ctx.currentScope.dump()
    ctx.currentClosure.dump()

    print("TODO: Codegen not implemented yet o(x_x)o")
    return 1

sys.exit(main(sys.argv))
