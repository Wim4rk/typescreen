#include "compose.h"
#include "utf8.h"
#include <stddef.h>

/* Each row: the accent's spacing form, then letter/result pairs. Covers
 * Latin-1 Supplement and Latin Extended-A. */
static const struct { uint32_t accent; const char *pairs; } table[] = {
    { 0x00B4 /* ´ acute */,
      "aáeéiíoóuúyýAÁEÉIÍOÓUÚYÝcćCĆnńNŃsśSŚzźZŹlĺLĹrŕRŔ" },
    { 0x0060 /* ` grave */,
      "aàeèiìoòuùAÀEÈIÌOÒUÙ" },
    { 0x005E /* ^ circumflex */,
      "aâeêiîoôuûAÂEÊIÎOÔUÛcĉCĈgĝGĜhĥHĤjĵJĴsŝSŜwŵWŴyŷYŶ" },
    { 0x00A8 /* ¨ diaeresis */,
      "aäeëiïoöuüyÿAÄEËIÏOÖUÜYŸ" },
    { 0x007E /* ~ tilde */,
      "aãnñoõAÃNÑOÕiĩIĨuũUŨ" },
    { 0x02DA /* ˚ ring */,
      "aåAÅuůUŮ" },
    { 0x02C7 /* ˇ caron */,
      "cčCČdďDĎeěEĚnňNŇrřRŘsšSŠtťTŤzžZŽ" },
    { 0x00B8 /* ¸ cedilla */,
      "cçCÇsşSŞtţTŢgģGĢkķKĶlļLĻnņNŅ" },
    { 0x02DD /* ˝ double acute */,
      "oőOŐuűUŰ" },
    { 0x02D8 /* ˘ breve */,
      "aăAĂgğGĞ" },
    { 0x02DB /* ˛ ogonek */,
      "aąAĄeęEĘiįIĮuųUŲ" },
    { 0x00AF /* ¯ macron */,
      "aāAĀeēEĒiīIĪoōOŌuūUŪ" },
    { 0x02D9 /* ˙ dot above */,
      "cċCĊeėEĖgġGĠzżZŻiıIİ" },
    { 0x002F /* / stroke */,
      "oøOØdđDĐlłLŁhħHĦtŧTŦ" },
};

uint32_t compose(uint32_t accent, uint32_t letter) {
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].accent != accent) continue;
        const char *s = table[i].pairs;
        while (*s) {
            uint32_t from = utf8_next(&s);
            if (!*s) break;
            uint32_t to = utf8_next(&s);
            if (from == letter) return to;
        }
        return 0;
    }
    return 0;
}
