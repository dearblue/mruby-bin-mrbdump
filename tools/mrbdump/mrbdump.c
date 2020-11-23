#include <mruby-aux.h>
#include <mruby-aux/dump.h>
#include <mruby-aux/option.h>
#include <mruby/compile.h>
#include <mruby/dump.h>
#include <mruby/irep.h>
#include <mruby/value.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#ifndef mrb_proc_p
# define mrb_proc_p(v) (mrb_type(v) == MRB_TT_PROC)
#endif

#define C(N) to_printable_char(N)

static inline int
to_printable_char(int n)
{
  return (ISPRINT(n) ? (n) : '.');
}

typedef int foreach_mrbfile_f(mrb_state *mrb, const char *path, struct RProc *proc, void *opaque);

static int
foreach_mrbfile(mrb_state *mrb, mrb_bool dump_result,
    const char *progname, int nfiles, const char *const files[],
    foreach_mrbfile_f *handler, void *opaque)
{
  int ai = mrb_gc_arena_save(mrb);
  mrbc_context *c = mrbc_context_new(mrb);
  c->dump_result = dump_result;
  c->no_exec = TRUE;

  int result = EXIT_SUCCESS;

  for (int i = nfiles; i > 0; i --, files ++) {
    if (strcmp(*files, "-") == 0) {
      printf("%s: %s is not support reads from stdin", *files, progname);
      result = EXIT_FAILURE;
      continue;
    }

    FILE *fp = fopen(*files, "rb");

    if (!fp) {
      printf("%s: %s\n", *files, strerror(errno));
      result = EXIT_FAILURE;
      continue;
    }

    char buf[sizeof(struct rite_binary_header)] = { 0 };

    fread(buf, sizeof(char), sizeof(struct rite_binary_header), fp);
    fseek(fp, 0, SEEK_END);

    if (!mrbx_taste_irep_buf(mrb, buf, ftell(fp), mrb_str_new_cstr(mrb, *files), FALSE)) {
      printf("<<file too short or version mismatch (or read from pipe) - %s>>\n", *files);
      result = EXIT_FAILURE;
      goto cleanup;
    }

    rewind(fp);
    printf("## %s\n", *files);

    mrb_value proc;
    proc = mrb_load_irep_file_cxt(mrb, fp, c);
    if (mrb_nil_p(proc)) {
      printf("%s: load error (not a mrb file or version mismatch)\n", *files);
      result = EXIT_FAILURE;
      goto cleanup;
    }

    if (!mrb_proc_p(proc)) {
      printf("%s: [[BUG]] no returns proc object by mrb_load_irep_file_cxt() [[BUG]]\n", *files);
      result = EXIT_FAILURE;
      goto cleanup;
    }

    fclose(fp);
    fp = NULL;

    if (handler) {
      int res = handler(mrb, *files, mrb_proc_ptr(proc), opaque);

      if (res < 0) {
        // 処理を打ち切る
        i = 0;
      } else if (res > 0) {
        result = EXIT_FAILURE;
      }
    }

cleanup:
    if (fp) {
      fclose(fp);
    }

    mrb_gc_arena_restore(mrb, ai);
  }

  mrbc_context_free(mrb, c);

  return result;
}

static int disasm_mrb_file(mrb_state *mrb, const char *progname, int nfiles, const char *const files[]);
static int print_file_header(mrb_state *mrb, const char *progname, int nfiles, const char *const files[]);
static int print_section_header(mrb_state *mrb, const char *progname, int nfiles, const char *const files[]);
static int print_ireps(mrb_state *mrb, const char *progname, int nfiles, const char *const files[]);
static int print_symbols(mrb_state *mrb, const char *progname, int nfiles, const char *const files[]);

static int
disasm_mrb_file(mrb_state *mrb, const char *progname, int nfiles, const char *const files[])
{
  return foreach_mrbfile(mrb, TRUE, progname, nfiles, files, NULL, NULL);
}

static int
print_file_header(mrb_state *mrb, const char *progname, int nfiles, const char *const files[])
{
  int ai = mrb_gc_arena_save(mrb);
  int result = EXIT_SUCCESS;
  bool first = true;

  for (int i = nfiles; i > 0; i --, files ++) {
    if (!first) {
      printf("\n");
    } else {
      first = false;
    }

    FILE *fp = fopen(*files, "rb");

    if (!fp) {
      printf("%s: %s\n", *files, strerror(errno));
      result = EXIT_FAILURE;
      continue;
    }

    char buf[sizeof(struct rite_binary_header)];
    if (fread(buf, sizeof(struct rite_binary_header), 1, fp) == 0) {
      printf("<<file too short - %s>>\n", *files);
      result = EXIT_FAILURE;
      continue;
    }

    const struct rite_binary_header *p = (const struct rite_binary_header *)buf;
    printf("## %s\n", *files);
    printf("%16s: %c%c%c%c (%02x %02x %02x %02x)\n", "binary_ident",
        C(p->binary_ident[0]), C(p->binary_ident[1]), C(p->binary_ident[2]), C(p->binary_ident[3]),
        p->binary_ident[0], p->binary_ident[1], p->binary_ident[2], p->binary_ident[3]);
    printf("%16s: %c%c%c%c (%02x %02x %02x %02x)\n", "binary_version",
#if MRUBY_RELEASE_NO < 30000
        C(p->binary_version[0]), C(p->binary_version[1]), C(p->binary_version[2]), C(p->binary_version[3]),
        p->binary_version[0], p->binary_version[1], p->binary_version[2], p->binary_version[3]);
#else
        C(p->major_version[0]), C(p->major_version[1]), C(p->minor_version[0]), C(p->minor_version[1]),
        p->major_version[0], p->major_version[1], p->minor_version[0], p->minor_version[1]);
#endif
    printf("%16s: 0x%02x%02x\n", "binary_crc",
        p->binary_crc[0], p->binary_crc[1]);
    printf("%16s: %" PRIu32 " (%02x %02x %02x %02x)\n", "binary_size",
        bin_to_uint32(p->binary_size), p->binary_size[0],
        p->binary_size[1], p->binary_size[2], p->binary_size[3]);
    printf("%16s: %c%c%c%c (%02x %02x %02x %02x)\n", "compiler_name",
        C(p->compiler_name[0]), C(p->compiler_name[1]), C(p->compiler_name[2]), C(p->compiler_name[3]),
        p->compiler_name[0], p->compiler_name[1], p->compiler_name[2], p->compiler_name[3]);
    printf("%16s: %c%c%c%c (%02x %02x %02x %02x)\n", "compiler_version",
        C(p->compiler_version[0]), C(p->compiler_version[1]), C(p->compiler_version[2]), C(p->compiler_version[3]),
        p->compiler_version[0], p->compiler_version[1], p->compiler_version[2], p->compiler_version[3]);

    fclose(fp);
    mrb_gc_arena_restore(mrb, ai);
  }

  return result;
}

static int
print_section_header(mrb_state *mrb, const char *progname, int nfiles, const char *const files[])
{
  return EXIT_FAILURE;
}

static int
print_ireps(mrb_state *mrb, const char *progname, int nfiles, const char *const files[])
{
  return EXIT_FAILURE;
}

static int
print_symbols(mrb_state *mrb, const char *progname, int nfiles, const char *const files[])
{
  return EXIT_FAILURE;
}

enum
{
  OPERATION_DISASSEMBLY,
  OPERATION_PRINT_FILEHEADER,
  OPERATION_PRINT_SECTIONHEADER,
  OPERATION_PRINT_IREPS,
  OPERATION_PRINT_SYMBOLS,
  OPERATION_PRINT_PROGVER,
};

#define OPTION_HELP '?'

int
main(int argc, char *argv[])
{
  static const mrbx_option_entry opts[] = {
    { -OPTION_HELP, NULL, "help", NULL },
    { 0, NULL, NULL, NULL },
    { 0, NULL, NULL, "operators:" },
    { 'd', NULL, NULL, "disassembly to stdout" },
    { 'f', NULL, NULL, "print file header (default)" },
    { 'h', NULL, NULL, "print section header" },
    { 'p', NULL, NULL, "print irep summary" },
    { 't', NULL, NULL, "print symbols" },
    { 'V', NULL, NULL, "print program version" },
  };

  mrbx_option opt = {
    argv[0], argc - 1, argv + 1,
    ELEMENTOF(opts), opts,
    NULL, NULL
  };

  int operation = OPERATION_PRINT_FILEHEADER;

  for (;;) {
    int t;

    switch (t = mrbx_option_parse(&opt)) {
    default:
      fprintf(stderr, "[BUG] unhandled option : <%c:0x%02x> [BUG]\n", C(t), t);
      exit(EXIT_FAILURE);
    case MRBX_OPTION_UNKNOWN:
      fprintf(stderr, "unknown option : <%c:0x%02x>\n", C(*opt.opt), (unsigned char)*opt.opt);
      exit(EXIT_FAILURE);
    case MRBX_OPTION_UNKNOWN_LONG:
      fprintf(stderr, "unknown option : <%s>\n", opt.opt);
      exit(EXIT_FAILURE);
    case OPTION_HELP:
      mrbx_option_usage(&opt, 0, 2, NULL, NULL, NULL);
      exit(EXIT_SUCCESS);
    case 'd':
      operation = OPERATION_DISASSEMBLY;
      continue;
    case 'f':
      operation = OPERATION_PRINT_FILEHEADER;
      continue;
    case 'h':
      operation = OPERATION_PRINT_SECTIONHEADER;
      continue;
    case 'p':
      operation = OPERATION_PRINT_IREPS;
      continue;
    case 't':
      operation = OPERATION_PRINT_SYMBOLS;
      continue;
    case 'V':
      operation = OPERATION_PRINT_PROGVER;
      continue;
    case MRBX_OPTION_END:
    case MRBX_OPTION_BREAK:
    case MRBX_OPTION_STOP:
      break;
    }

    break;
  }

  if (operation == OPERATION_PRINT_PROGVER) {
    printf("%s with %s\n", opt.progname, MRUBY_DESCRIPTION);
    exit(EXIT_SUCCESS);
  }

  if (opt.argc < 1) {
    fprintf(stderr, "%s: no given files\n", opt.progname);
    fprintf(stderr, "\ttype '%s --help' if you want to print help\n", opt.progname);
    exit(EXIT_FAILURE);
  }

  mrb_state *mrb = mrb_open();
  int status;

  switch (operation) {
  case OPERATION_DISASSEMBLY:
    status = disasm_mrb_file(mrb, opt.progname, opt.argc, (const char *const *)opt.argv);
    break;
  case OPERATION_PRINT_FILEHEADER:
    status = print_file_header(mrb, opt.progname, opt.argc, (const char *const *)opt.argv);
    break;
  case OPERATION_PRINT_SECTIONHEADER:
    status = print_section_header(mrb, opt.progname, opt.argc, (const char *const *)opt.argv);
    break;
  case OPERATION_PRINT_IREPS:
    status = print_ireps(mrb, opt.progname, opt.argc, (const char *const *)opt.argv);
    break;
  case OPERATION_PRINT_SYMBOLS:
    status = print_symbols(mrb, opt.progname, opt.argc, (const char *const *)opt.argv);
    break;
  }

  mrb_close(mrb);

  return status;
}
