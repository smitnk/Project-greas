/* L2 host test: Blender 3.6.23's .blend reader FileReaders (blenlib/intern/filereader_*.c) on the
 * pinned zstd 1.5.0 / zlib 1.2.13. Inputs are written here exactly as Blender's writefile.cc writes
 * a compressed .blend (1 MiB frames, ZSTD_compress level 3, seek-table skippable frame), plus a
 * zstd stream without a seek table and a gzip file, then read and seeked through Blender's readers. */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <zstd.h>

#include "BLI_fileops.h"
#include "BLI_filereader.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

enum { CHUNK = 1 << 20, LEVEL = 3 }; /* writefile.cc ZSTD_CHUNK_SIZE / ZSTD_COMPRESSION_LEVEL */

static void put_u32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); } /* little-endian hosts only */

/* writefile.cc ww_write_zstd + zstd_write_seekable_frames (test-side writer, not product code). */
static void write_zstd(const char *path, const unsigned char *data, size_t len, int seek_table)
{
  FILE *f = fopen(path, "wb");
  uint32_t csz[64], usz[64], n = 0;
  for (size_t ofs = 0; ofs < len; ofs += CHUNK, n++) {
    const size_t sz = len - ofs < CHUNK ? len - ofs : CHUNK;
    const size_t bound = ZSTD_compressBound(sz);
    void *out = malloc(bound);
    const size_t c = ZSTD_compress(out, bound, data + ofs, sz, LEVEL);
    fwrite(out, 1, c, f);
    free(out);
    csz[n] = (uint32_t)c;
    usz[n] = (uint32_t)sz;
  }
  if (seek_table) {
    put_u32(f, 0x184D2A5E);
    put_u32(f, n * 8 + 9);
    for (uint32_t i = 0; i < n; i++) {
      put_u32(f, csz[i]);
      put_u32(f, usz[i]);
    }
    put_u32(f, n);
    fputc(0, f);
    put_u32(f, 0x8F92EAB1);
  }
  fclose(f);
}

static FileReader *open_reader(const char *path, int kind)
{
  const int fd = open(path, O_RDONLY);
  FileReader *raw = BLI_filereader_new_file(fd);
  return kind == 'z' ? BLI_filereader_new_zstd(raw) : kind == 'g' ? BLI_filereader_new_gzip(raw) : raw;
}

static void check_full_read(FileReader *r, const unsigned char *data, size_t len, const char *what)
{
  unsigned char *buf = malloc(len + 16);
  size_t got = 0;
  for (ssize_t n; (n = r->read(r, buf + got, 100003)) > 0;) {
    got += (size_t)n;
  }
  printf("  %s: read %zu of %zu bytes\n", what, got, len);
  CHECK(got == len && memcmp(buf, data, len) == 0, what);
  free(buf);
}

int main(int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : ".";
  const size_t len = 5 * CHUNK + 12345; /* 6 frames, the last one partial */
  unsigned char *data = malloc(len);
  uint32_t x = 0x9E3779B9u;
  for (size_t i = 0; i < len; i++) { /* compressible but not trivial */
    x = x * 1664525u + 1013904223u;
    data[i] = (unsigned char)((i % 251) ^ ((x >> 24) & 7));
  }
  char p_seek[512], p_plain[512], p_raw[512], p_gz[512];
  snprintf(p_seek, sizeof(p_seek), "%s/seek.zst", dir);
  snprintf(p_plain, sizeof(p_plain), "%s/plain.zst", dir);
  snprintf(p_raw, sizeof(p_raw), "%s/raw.bin", dir);
  snprintf(p_gz, sizeof(p_gz), "%s/raw.bin.gz", dir);
  write_zstd(p_seek, data, len, 1);
  write_zstd(p_plain, data, len, 0);
  FILE *f = fopen(p_raw, "wb");
  fwrite(data, 1, len, f);
  fclose(f);
  char cmd[1200];
  snprintf(cmd, sizeof(cmd), "gzip -k -f -6 '%s'", p_raw);
  CHECK(system(cmd) == 0, "gzip fixture");

  /* Blender-written seekable zstd: seek support and random access. */
  FileReader *r = open_reader(p_seek, 'z');
  CHECK(r && r->seek, "seek-table zstd reader has seek");
  if (r && r->seek) {
    check_full_read(r, data, len, "zstd seek-table full read");
    const off64_t probes[] = {0, 1, CHUNK - 3, CHUNK, 3 * CHUNK + 777, (off64_t)len - 10};
    for (int i = 0; i < 6; i++) {
      unsigned char b[64] = {0};
      const off64_t at = r->seek(r, probes[i], SEEK_SET);
      const ssize_t n = r->read(r, b, 8);
      CHECK(at == probes[i] && n == 8 && memcmp(b, data + probes[i], 8) == 0, "zstd seek+read");
    }
    CHECK(r->seek(r, -4, SEEK_END) == (off64_t)len - 4, "zstd SEEK_END");
    CHECK(r->seek(r, 1, SEEK_END) == -1, "zstd seek past end rejected");
    r->close(r);
  }
  /* zstd without seek table: streaming only, as in Blender. */
  r = open_reader(p_plain, 'z');
  CHECK(r && r->seek == NULL, "plain zstd reader is not seekable");
  if (r) {
    check_full_read(r, data, len, "zstd stream full read");
    r->close(r);
  }
  /* gzip: streaming only, as in Blender. */
  r = open_reader(p_gz, 'g');
  CHECK(r && r->seek == NULL, "gzip reader is not seekable");
  if (r) {
    check_full_read(r, data, len, "gzip full read");
    r->close(r);
  }
  /* readfile.cc picks the reader from the first 4 bytes with Blender's magic tests. */
  const char *paths[3] = {p_seek, p_gz, p_raw};
  for (int i = 0; i < 3; i++) {
    char h[4] = {0};
    FILE *hf = fopen(paths[i], "rb");
    CHECK(fread(h, 1, 4, hf) == 4, "read header");
    fclose(hf);
    CHECK(BLI_file_magic_is_zstd(h) == (i == 0), "zstd magic");
    CHECK(BLI_file_magic_is_gzip(h) == (i == 1), "gzip magic");
  }
  /* Uncompressed: file reader, mmap reader (readfile.cc's first choice) and memory reader. */
  r = open_reader(p_raw, 'f');
  check_full_read(r, data, len, "raw file full read");
  r->close(r);
  int fd = open(p_raw, O_RDONLY);
  r = BLI_filereader_new_mmap(fd);
  CHECK(r != NULL, "mmap reader");
  if (r) {
    check_full_read(r, data, len, "mmap full read");
    CHECK(r->seek(r, 3 * CHUNK, SEEK_SET) == 3 * CHUNK, "mmap seek");
    r->close(r);
  }
  close(fd);
  r = BLI_filereader_new_memory(data, len);
  check_full_read(r, data, len, "memory full read");
  r->close(r);
  free(data);
  printf(failures ? "%d FAILURES\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
