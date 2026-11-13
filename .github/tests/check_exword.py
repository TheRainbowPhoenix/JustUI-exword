from pathlib import Path
import subprocess
import tempfile
import re
import sys

ui = Path(__file__).resolve().parents[2]
root = Path(sys.argv[1]).resolve()

with tempfile.TemporaryDirectory() as directory:
    p = Path(directory)
    (p / 'gint').mkdir()
    (p / 'justui').mkdir()

    # Generate gint/config.h
    s = (root / 'include/gint/config.h.in').read_text()
    s = re.sub(r'^#cmakedefine.*$', '', s, flags=re.M)
    (p / 'gint/config.h').write_text(s)

    # Generate justui/config.h
    justui_config = (ui / 'include/justui/config.h.in').read_text()
    (p / 'justui/config.h').write_text(justui_config.replace('@JustUI_VERSION@', 'test'))

    # Generate stub headers
    (p / 'openlibm.h').write_text('#include <math.h>\n')
    
    dirent_h = (
        'struct dirent { unsigned char d_type; char d_name[256]; };\n'
        'typedef struct DIR DIR;\n'
        '#define DT_DIR 4\n'
        'DIR *opendir(char const *);\n'
        'struct dirent *readdir(DIR *);\n'
        'int closedir(DIR *);\n'
    )
    (p / 'dirent.h').write_text(dirent_h)

    # Compiler arguments
    args = [
        'gcc', '-std=gnu11', '-DFXEXWORD',
        '-I' + str(p),
        '-I' + str(root / 'include'),
        '-I' + str(ui / 'include'),
        '-Wall', '-Wextra'
    ]

    # Syntax check all source files
    failed = False
    for f in (ui / 'src').glob('*.c'):
        result = subprocess.run(
            args + ['-fsyntax-only', str(f)],
            capture_output=True,
            text=True
        )
        if result.returncode:
            print(f.name, result.stderr)
            failed = True

    if failed:
        sys.exit(1)

    print('JustUI EX-word: all source files pass host syntax checks')

    # Generate and run keymap test
    f = p / 'keymap_test.c'
    keymap_c_path = str(ui / 'src/keymap.c').replace('\\', '/')
    
    keymap_test_code = (
        '#include <assert.h>\n'
        f'#include "{keymap_c_path}"\n'
        'int main(void) {\n'
        '    for (int i = KEY_EX_Q; i <= KEY_EX_M; i++) {\n'
        '        unsigned c = keymap_translate(i, 0, 0);\n'
        '        assert(c >= 97 && c <= 122);\n'
        '        assert(keymap_translate(i, 1, 0) == c - 32);\n'
        '        assert(keymap_translate(i, 0, 1) == c);\n'
        '    }\n'
        '    assert(keymap_translate(KEY_EXIT, 0, 0) == 0);\n'
        '}\n'
    )
    f.write_text(keymap_test_code)

    subprocess.run(args + [str(f), '-o', str(p / 'test.exe')], check=True)
    subprocess.run([str(p / 'test.exe')], check=True)

    print('JustUI EX-word: QWERTY input and modifiers passed')