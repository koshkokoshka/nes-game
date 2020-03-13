#include <nes.h>

/* Base types
 * ------------------------------------------------------------------------- */
typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;

/* Helpers
* ------------------------------------------------------------------------- */
#define HI(x) ((x) >> 8)
#define LO(x) ((x)  & 0xFF)

#define WriteToRegister(reg, val) { asm("LDA #%b", val); asm("STA %w", reg); }

/* INES header
 * ------------------------------------------------------------------------- */
#pragma data-name(push, "HEADER")

struct INES_Header
{
    char signature[4];
    u8   romsPRG16;             /* number of 16k PRG-ROM banks */
    u8   romsCHR8;              /* number of  8k CHR-ROM banks */
    u8   flags6;                /* cartridge type LSB flags */
    u8   flags7;                /* cartridge type MSB flags */
    u8   ramsPRG8;              /* number of  8k PRG-RAM banks */
    u8   flags9;
    char reserved[6];

} header = {
    { 0x4E, 0x45, 0x53, 0x1A },     /* signature = "NES"^z */
    2,
    1
};

#pragma data-name(pop)

/* Data
* ------------------------------------------------------------------------- */
#pragma bss-name(push, "CHARS")

extern u8 data[];

#pragma bss-name(pop)

/* Globals
* ------------------------------------------------------------------------- */
#pragma bss-name(push, "ZEROPAGE")

struct // temporary storage
{
    // 32 bit
    u8 i;
    u8 j;
    u8 k;
    u8 l;

    // 32 bit
    u8 x;
    u8 y;
    u8 w;
    u8 h;

    // 32 bit
    u8 x0;
    u8 y0;
    u8 x1;
    u8 y1;

    // 32 bit
    u16 aw;
    u16 bw;

    // total = 128 bits

} tmp;

u8 P1 = 0;

#pragma bss-name(pop)

/* Sprites
* ------------------------------------------------------------------------- */
#pragma bss-name(push, "BSS")

typedef struct
{
    u8 y; // y position
    u8 i; // tile number
    u8 a; // attributes
    u8 x; // x position

} Sprite;

#pragma bss-name(pop)

/* PPU
 * ------------------------------------------------------------------------- */

/* PPU ports */
#define PPU_CTRL                       (u16)(0x2000)
#define PPU_MASK                       (u16)(0x2001)
#define PPU_STAT                       (u16)(0x2002)
#define OAM_ADDR                       (u16)(0x2003)
#define OAM_DATA                       (u16)(0x2004)
#define PPU_SCRL                       (u16)(0x2005)
#define PPU_ADDR                       (u16)(0x2006)
#define PPU_DATA                       (u16)(0x2007)
#define OAM_DMA                        (u16)(0x4014)

/* PPU_MASK flags */
#define PPU_MASK_F0_COLOR                 (u8)(0x00)
#define PPU_MASK_F0_GRAY                  (u8)(0x01)
#define PPU_MASK_F1_BG_L8_SHOW            (u8)(0x02)
#define PPU_MASK_F1_BG_L8_HIDE            (u8)(0x00)
#define PPU_MASK_F2_FG_L8_SHOW            (u8)(0x04)
#define PPU_MASK_F2_FG_L8_HIDE            (u8)(0x00)
#define PPU_MASK_F3_BG_SHOW               (u8)(0x08)
#define PPU_MASK_F3_BG_HIDE               (u8)(0x00)
#define PPU_MASK_F4_FG_SHOW               (u8)(0x10)
#define PPU_MASK_F4_FG_HIDE               (u8)(0x00)
#define PPU_MASK_F5_EM_R                  (u8)(0x20)
#define PPU_MASK_F6_EM_G                  (u8)(0x40)
#define PPU_MASK_F7_EM_B                  (u8)(0x80)

/* PPU_CTRL flags */
#define PPU_CTRL_F0_NAMETABLE_0           (u8)(0x00)
#define PPU_CTRL_F0_NAMETABLE_1           (u8)(0x01)
#define PPU_CTRL_F0_NAMETABLE_2           (u8)(0x02)
#define PPU_CTRL_F0_NAMETABLE_3           (u8)(0x03)
#define PPU_CTRL_F1_INC_1                 (u8)(0x00)
#define PPU_CTRL_F1_INC_32                (u8)(0x04)
#define PPU_CTRL_F2_FG_TABLE_0            (u8)(0x00)
#define PPU_CTRL_F2_FG_TABLE_1            (u8)(0x08)
#define PPU_CTRL_F3_BG_TABLE_0            (u8)(0x00)
#define PPU_CTRL_F3_BG_TABLE_1            (u8)(0x10)
#define PPU_CTRL_F4_SPRITE_SIZE_8X8       (u8)(0x00)
#define PPU_CTRL_F4_SPRITE_SIZE_8x16      (u8)(0x20)
#define PPU_CTRL_F5_PPU_MASTER            (u8)(0x00)
#define PPU_CTRL_F5_PPU_SLAVE             (u8)(0x40)
#define PPU_CTRL_F6_NMI_DISABLE           (u8)(0x00)
#define PPU_CTRL_F6_NMI_ENABLE            (u8)(0x80)

/* PPU_ADDR common values */
#define PPU_ADDR_NAMETABLE_START       (u16)(0x2000)
#define PPU_ADDR_NAMETABLE_END         (u16)(0x23FF)
#define PPU_ADDR_PALETTE_START         (u16)(0x3F00)
#define PPU_ADDR_PALETTE_END           (u16)(0x3F1F)
#define PPU_ADDR_OAM                   (u16)(0x2100)

#define PPU_SetAddr(x) \
    *((u8 *)PPU_ADDR) = HI(x); \
    *((u8 *)PPU_ADDR) = LO(x)

#define PPU_Enable(flags, mask) \
    WriteToRegister(PPU_CTRL, flags); \
    WriteToRegister(PPU_MASK, mask)

static void PPU_VBankWait()
{
    asm("BIT %w", PPU_STAT);
    asm("BPL %v", PPU_VBankWait);
}

static void _FillRect(void)
{
    tmp.aw = (u16)0x2000 + tmp.x;

    /* We can't easy multiply byte Y by 32, because of byte value limited to 255.
     * So, there is a trick to get the correct address... */
    tmp.j = tmp.y;
    while (tmp.j >= (u8)0x08) {
        tmp.aw += (u16)0x100;
        tmp.j  -= (u8)0x08;
    }
    tmp.aw += (u8)(tmp.j * 32);

    for (tmp.j = 0; tmp.j < tmp.h; ++tmp.j) {
        PPU_SetAddr((u16)tmp.aw);
        for (tmp.k = 0; tmp.k < tmp.w; ++tmp.k) {
            *((u8 *)PPU_DATA) = 0;
        }
        tmp.aw += (u8)0x20;
    }
}

#define FillRect(_x, _y, _w, _h, _tile) \
{ \
    tmp.x = (_x); \
    tmp.y = (_y); \
    tmp.w = (_w); \
    tmp.h = (_h); \
    tmp.i = (_tile); \
    _FillRect(); \
}

const char str[] = "HARDWORK";

static void ShowDialog()
{
    tmp.x = 10;
    tmp.y = 8;

    tmp.w = 5;
    tmp.h = 3;

    // ---

    tmp.aw = (u16)0x2000;

    while (tmp.y >= (u8)0x08) {
        tmp.aw += (u16)0x100;
        tmp.y  -= (u8)0x08;
    }
    tmp.aw += (u8)(tmp.y * 32);
    tmp.aw +=      tmp.x;
    PPU_SetAddr((u16)tmp.aw);

    tmp.k = 0x65;
//    tmp.k = 0x68;
//    tmp.k = 0x6b;

    *((u8 *)PPU_DATA) = tmp.k++; // top-left border
    tmp.i = tmp.w;
    while (--tmp.i) {
        *((u8 *)PPU_DATA) = tmp.k; // top border
    }
    *((u8 *)PPU_DATA) = ++tmp.k; // top-right border

    tmp.k += 0x0e;
    tmp.l = 0;

    tmp.i = tmp.h;
    while (--tmp.i) {
        tmp.aw += (u8)0x20;
        PPU_SetAddr((u16)tmp.aw);

        *((u8 *)PPU_DATA) = tmp.k; // left border
        tmp.j = tmp.w;
        while (--tmp.j) {
            *((u8 *)PPU_DATA) = str[tmp.l];
            tmp.l++;
        }
        tmp.k += 0x02;
        *((u8 *)PPU_DATA) = tmp.k; // right border
        tmp.k -= 0x02;
    }

    tmp.aw += (u8)0x20;
    PPU_SetAddr((u16)tmp.aw);

    tmp.k += 0x10;
    *((u8 *)PPU_DATA) = tmp.k++; // bottom-left border
    tmp.i = tmp.w;
    while (--tmp.i) {
        *((u8 *)PPU_DATA) = tmp.k; // bottom border
    }
    *((u8 *)PPU_DATA) = ++tmp.k; // bottom-right border


    PPU_SetAddr(0x0000);
}

void SetPalette0()
{
    PPU_SetAddr(0x3F00);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x00);
    WriteToRegister(PPU_DATA, 0x10);
    WriteToRegister(PPU_DATA, 0x20);
    PPU_SetAddr(0x3F10);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x00);
    WriteToRegister(PPU_DATA, 0x10);
    WriteToRegister(PPU_DATA, 0x20);
}

void SetPalette1()
{
    PPU_SetAddr(0x3F00);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x00);
    WriteToRegister(PPU_DATA, 0x10);
    PPU_SetAddr(0x3F10);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x00);
    WriteToRegister(PPU_DATA, 0x10);
}

void SetPalette2()
{
    PPU_SetAddr(0x3F00);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x00);
    PPU_SetAddr(0x3F10);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x00);
}

void SetPalette3()
{
    PPU_SetAddr(0x3F00);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    PPU_SetAddr(0x3F10);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
    WriteToRegister(PPU_DATA, 0x0F);
}

static const u8 player_sprite_d[] = {
        // sprite, attribute
        0x99, 0x00,
        0x99, 0x40,
        0xA9, 0x00,
        0xA9, 0x40,
        0xB9, 0x00,
        0xB9, 0x40,
        0xC9, 0x00,
        0xC9, 0x40
};
static const u8 player_sprite_r[] = {
        // sprite, attribute
        0x9E, 0x40,
        0x99, 0x40,
        0xAE, 0x40,
        0xAD, 0x40,
        0xBE, 0x40,
        0xBD, 0x40,
        0xC9, 0x00,
        0xCD, 0x40
};
static const u8 player_sprite_l[] = {
        // sprite, attribute
        0x99, 0x00,
        0x9E, 0x00,
        0xAD, 0x00,
        0xAE, 0x00,
        0xBD, 0x00,
        0xBE, 0x00,
        0xCD, 0x00,
        0xC9, 0x40
};
static const u8 player_sprite_u[] = {
        // sprite, attribute
        0x9B, 0x00,
        0x9B, 0x40,
        0xAB, 0x00,
        0xAB, 0x40,
        0xBB, 0x00,
        0xBB, 0x40,
        0xC9, 0x00,
        0xC9, 0x40
};

void Player_LoadSprite0()
{
    ((Sprite *)0x0204)->i = *((u8 *)player_sprite_d + 0);  ((Sprite *)0x0204)->a = *((u8 *)player_sprite_d + 1);
    ((Sprite *)0x0208)->i = *((u8 *)player_sprite_d + 2);  ((Sprite *)0x0208)->a = *((u8 *)player_sprite_d + 3);
    ((Sprite *)0x020C)->i = *((u8 *)player_sprite_d + 4);  ((Sprite *)0x020C)->a = *((u8 *)player_sprite_d + 5);
    ((Sprite *)0x0210)->i = *((u8 *)player_sprite_d + 6);  ((Sprite *)0x0210)->a = *((u8 *)player_sprite_d + 7);
    ((Sprite *)0x0214)->i = *((u8 *)player_sprite_d + 8);  ((Sprite *)0x0214)->a = *((u8 *)player_sprite_d + 9);
    ((Sprite *)0x0218)->i = *((u8 *)player_sprite_d + 10); ((Sprite *)0x0218)->a = *((u8 *)player_sprite_d + 11);
    ((Sprite *)0x021C)->i = *((u8 *)player_sprite_d + 12); ((Sprite *)0x021C)->a = *((u8 *)player_sprite_d + 13);
    ((Sprite *)0x0220)->i = *((u8 *)player_sprite_d + 14); ((Sprite *)0x0220)->a = *((u8 *)player_sprite_d + 15);
}

void Player_LoadSprite1()
{
    ((Sprite *)0x0204)->i = *((u8 *)player_sprite_u + 0);  ((Sprite *)0x0204)->a = *((u8 *)player_sprite_u + 1);
    ((Sprite *)0x0208)->i = *((u8 *)player_sprite_u + 2);  ((Sprite *)0x0208)->a = *((u8 *)player_sprite_u + 3);
    ((Sprite *)0x020C)->i = *((u8 *)player_sprite_u + 4);  ((Sprite *)0x020C)->a = *((u8 *)player_sprite_u + 5);
    ((Sprite *)0x0210)->i = *((u8 *)player_sprite_u + 6);  ((Sprite *)0x0210)->a = *((u8 *)player_sprite_u + 7);
    ((Sprite *)0x0214)->i = *((u8 *)player_sprite_u + 8);  ((Sprite *)0x0214)->a = *((u8 *)player_sprite_u + 9);
    ((Sprite *)0x0218)->i = *((u8 *)player_sprite_u + 10); ((Sprite *)0x0218)->a = *((u8 *)player_sprite_u + 11);
    ((Sprite *)0x021C)->i = *((u8 *)player_sprite_u + 12); ((Sprite *)0x021C)->a = *((u8 *)player_sprite_u + 13);
    ((Sprite *)0x0220)->i = *((u8 *)player_sprite_u + 14); ((Sprite *)0x0220)->a = *((u8 *)player_sprite_u + 15);
}

void Player_LoadSprite2()
{
    ((Sprite *)0x0204)->i = *((u8 *)player_sprite_r + 0);  ((Sprite *)0x0204)->a = *((u8 *)player_sprite_r + 1);
    ((Sprite *)0x0208)->i = *((u8 *)player_sprite_r + 2);  ((Sprite *)0x0208)->a = *((u8 *)player_sprite_r + 3);
    ((Sprite *)0x020C)->i = *((u8 *)player_sprite_r + 4);  ((Sprite *)0x020C)->a = *((u8 *)player_sprite_r + 5);
    ((Sprite *)0x0210)->i = *((u8 *)player_sprite_r + 6);  ((Sprite *)0x0210)->a = *((u8 *)player_sprite_r + 7);
    ((Sprite *)0x0214)->i = *((u8 *)player_sprite_r + 8);  ((Sprite *)0x0214)->a = *((u8 *)player_sprite_r + 9);
    ((Sprite *)0x0218)->i = *((u8 *)player_sprite_r + 10); ((Sprite *)0x0218)->a = *((u8 *)player_sprite_r + 11);
    ((Sprite *)0x021C)->i = *((u8 *)player_sprite_r + 12); ((Sprite *)0x021C)->a = *((u8 *)player_sprite_r + 13);
    ((Sprite *)0x0220)->i = *((u8 *)player_sprite_r + 14); ((Sprite *)0x0220)->a = *((u8 *)player_sprite_r + 15);
}

void Player_LoadSprite3()
{
    ((Sprite *)0x0204)->i = *((u8 *)player_sprite_l + 0);  ((Sprite *)0x0204)->a = *((u8 *)player_sprite_l + 1);
    ((Sprite *)0x0208)->i = *((u8 *)player_sprite_l + 2);  ((Sprite *)0x0208)->a = *((u8 *)player_sprite_l + 3);
    ((Sprite *)0x020C)->i = *((u8 *)player_sprite_l + 4);  ((Sprite *)0x020C)->a = *((u8 *)player_sprite_l + 5);
    ((Sprite *)0x0210)->i = *((u8 *)player_sprite_l + 6);  ((Sprite *)0x0210)->a = *((u8 *)player_sprite_l + 7);
    ((Sprite *)0x0214)->i = *((u8 *)player_sprite_l + 8);  ((Sprite *)0x0214)->a = *((u8 *)player_sprite_l + 9);
    ((Sprite *)0x0218)->i = *((u8 *)player_sprite_l + 10); ((Sprite *)0x0218)->a = *((u8 *)player_sprite_l + 11);
    ((Sprite *)0x021C)->i = *((u8 *)player_sprite_l + 12); ((Sprite *)0x021C)->a = *((u8 *)player_sprite_l + 13);
    ((Sprite *)0x0220)->i = *((u8 *)player_sprite_l + 14); ((Sprite *)0x0220)->a = *((u8 *)player_sprite_l + 15);
}

static u8 Player_direction = 0;

void Player_OnDirectionChange()
{
    switch (Player_direction) {
        case 0: Player_LoadSprite0(); break;
        case 1: Player_LoadSprite1(); break;
        case 2: Player_LoadSprite2(); break;
        case 3: Player_LoadSprite3(); break;
    }
}

static const u8 room1_x = 5;
static const u8 room1_y = 6;

static const u8 room1[16 * 13] = {
        0x62, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x64, 0x00, 0x00,
        0x72, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x61, 0x71, 0x71, 0x61, 0x73, 0x00, 0x00,
        0x72, 0x01, 0x98, 0x61, 0x61, 0xC0, 0xC1, 0x61, 0x61, 0x61, 0x90, 0x91, 0x61, 0x73, 0x00, 0x00,
        0x72, 0x01, 0xA8, 0x61, 0x61, 0xD0, 0xD1, 0x61, 0x61, 0x61, 0xA0, 0xA1, 0x61, 0x73, 0x00, 0x00,
        0x72, 0x01, 0xB8, 0x61, 0xA2, 0xA3, 0xA4, 0xA5, 0x61, 0x61, 0xB0, 0xB1, 0x61, 0x73, 0x00, 0x00,
        0x72, 0x01, 0xC8, 0x71, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0x71, 0xA0, 0xA1, 0x71, 0x73, 0x00, 0x00,
        0x72, 0xD7, 0xD8, 0x81, 0xC2, 0x03, 0x03, 0xC5, 0xC6, 0x81, 0x81, 0x81, 0x81, 0x73, 0x00, 0x00,
        0x82, 0x81, 0x81, 0x81, 0xD2, 0xD3, 0xD3, 0xD5, 0x81, 0x81, 0x81, 0x81, 0x81, 0x83, 0x00, 0x00,
        0x82, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x83, 0x00, 0x00,
        0x82, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x83, 0x00, 0x00,
        0x82, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x83, 0x00, 0x00,
        0x82, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x83, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static u8 Player_x = 0;
static u8 Player_y = 0;

static u8 Player_collision_L = 0;
static u8 Player_collision_R = 0;
static u8 Player_collision_U = 0;
static u8 Player_collision_D = 0;

void Player_CheckCollisionU()
{
    tmp.x = ((Sprite *)0x021C)->x;
    tmp.y = ((Sprite *)0x021C)->y - 1;

    tmp.x -= room1_x << 3;
    tmp.y -= room1_y << 3;

    tmp.x >>= 3; // bit shift by 3 = divide by 8
    tmp.y >>= 3;

    tmp.w = 2;
    tmp.h = 1;


    Player_collision_U = 0x81;

    tmp.l  = tmp.y << 4;
    tmp.l += tmp.x;

    for (tmp.j = 0; tmp.j < tmp.w; ++tmp.j) {

        tmp.i = room1[tmp.l];
        if (tmp.i != 0x81) {
            Player_collision_U = 0;
            break;
        }
        tmp.l++;
    }
}
void Player_CheckCollisionD()
{
    tmp.x = ((Sprite *)0x021C)->x;
    tmp.y = ((Sprite *)0x021C)->y + 1;

    tmp.x -= room1_x << 3;
    tmp.y -= room1_y << 3;

    tmp.x >>= 3; // bit shift by 3 = divide by 8
    tmp.y >>= 3;

    tmp.w = 2;
    tmp.h = 1;


    Player_collision_D = 0x81;

    tmp.l  = (tmp.y + tmp.h) << 4;
    tmp.l +=  tmp.x;

    for (tmp.j = 0; tmp.j < tmp.w; ++tmp.j) {

        tmp.i = room1[tmp.l];
        if (tmp.i != 0x81) {
            Player_collision_D = 0;
            break;
        }
        tmp.l++;
    }
}
void Player_CheckCollisionL()
{
    tmp.x = ((Sprite *)0x021C)->x - 1;
    tmp.y = ((Sprite *)0x021C)->y;

    tmp.x -= room1_x << 3;
    tmp.y -= room1_y << 3;

    tmp.x >>= 3; // bit shift by 3 = divide by 8
    tmp.y >>= 3;

    tmp.w = 2;
    tmp.h = 1;


    tmp.l  = tmp.y << 4; // bit shift by 4 = multiply by 16
    tmp.l += tmp.x;

    tmp.i = room1[tmp.l];

    Player_collision_L = tmp.i;
}
void Player_CheckCollisionR()
{
    tmp.x = ((Sprite *)0x021C)->x;
    tmp.y = ((Sprite *)0x021C)->y;

    tmp.x -= room1_x << 3;
    tmp.y -= room1_y << 3;

    tmp.x >>= 3; // bit shift by 3 = divide by 8
    tmp.y >>= 3;

    tmp.w = 2;
    tmp.h = 1;


    tmp.l  = tmp.y << 4;
    tmp.l += tmp.x;
    tmp.l += tmp.w;

    tmp.i = room1[tmp.l];

    Player_collision_R = tmp.i;
}

void Player_CheckCollision()
{
//    tmp.w += tmp.x;
//    tmp.h += tmp.y;

//    tmp.w = tmp.x1 - tmp.x0;
//    tmp.h = tmp.y1 - tmp.y0;

    Player_CheckCollisionD();
    Player_CheckCollisionL();
    Player_CheckCollisionR();
    Player_CheckCollisionU();
}

/* Startup code
 * ------------------------------------------------------------------------- */
#pragma code-name(push, "STARTUP")

void RST_Handler()
{
//    asm("LDA #$01");
//    asm("STA $4015");
//    asm("LDA #$9F");
//    asm("STA $4000");
//    asm("LDA #$22");
//    asm("STA $4003");
//
//    asm("LDA #$80");
//    asm("STA $2001");

    asm("SEI"); // disable IRQ
    asm("CLD"); // disable decimal mode

    // disable APU
    asm("LDX #$40");
    asm("STX $4017");

    // setup stack
    asm("LDX #$FF");
    asm("TXS");

    /* PPU_Disable */
    asm("INX"); /* now X = 0 */
    asm("STX %w", PPU_CTRL); // disable NMI
    asm("STX %w", PPU_MASK); // disable PPU
    asm("STX $4010");        // disable DMC

    PPU_VBankWait();  /* warm up PPU */

    /* have some time between two VBanks to clean up memory */
//    asm("clrmem:");
//    asm("LDA #$00");
//    asm("STA $0000, x");
//    asm("STA $0100, x");
//    asm("STA $0300, x");
//    asm("STA $0400, x");
//    asm("STA $0500, x");
//    asm("STA $0600, x");
//    asm("STA $0700, x");
//    asm("LDA #$FE    ");
//    asm("STA $0200, x");
//    asm("INX");
//    asm("BNE clrmem");

    PPU_VBankWait(); /* PPU is ready after this VBank */

    SetPalette0();

//    for (tmp.i = 0; tmp.i < 32; ++tmp.i) {
//        *((u8 *)PPU_DATA) = palette[tmp.i];
//    }

    /* load sprites and tiles */
//    PPU_SetAddr(0x2190);
//    for (i = 0; i < 192; ++i) {
//        PPU_DATA_REG = 0x0;
//    }

//    PPU_SetAddr(0x2000);
//    PPU_DATA_REG = 0x90;

    //
    // Background
    //
    /* draw top ui */
    FillRect( 1, 22, 30,  1, 0x04);

    tmp.x = room1_x;
    tmp.y = room1_y;
    tmp.w = 14;
    tmp.h = 12;

    tmp.aw = (u16)PPU_ADDR_NAMETABLE_START + tmp.x;

    /* we can't easy multiply byte Y by 32 because of byte value limit to 255
     * so there is a trick to get the correct address: */
    tmp.j = tmp.y;
    while (tmp.j >= (u8)0x08) {
        tmp.aw += (u16)0x100;
        tmp.j  -= (u8)0x08;
    }
    tmp.aw += (u8)(tmp.j * 32);

    for (tmp.j = 0; tmp.j < tmp.h; ++tmp.j) // y
    {
        PPU_SetAddr((u16)tmp.aw);
        tmp.aw += (u8)0x20; // next row

        for (tmp.k = 0; tmp.k < tmp.w; ++tmp.k) // x
        {
            tmp.l = (tmp.j << 4) + tmp.k;
            tmp.i = room1[tmp.l];

            *((u8 *)PPU_DATA) = tmp.i;
        }
    }

    //
    // Foreground
    //
    // y, tile, palette, x
    ((Sprite *)0x0204)->y = 0x60; ((Sprite *)0x0204)->x = 0x70;
    ((Sprite *)0x0208)->y = 0x60; ((Sprite *)0x0208)->x = 0x78;
    ((Sprite *)0x020C)->y = 0x68; ((Sprite *)0x020C)->x = 0x70;
    ((Sprite *)0x0210)->y = 0x68; ((Sprite *)0x0210)->x = 0x78;
    ((Sprite *)0x0214)->y = 0x70; ((Sprite *)0x0214)->x = 0x70;
    ((Sprite *)0x0218)->y = 0x70; ((Sprite *)0x0218)->x = 0x78;
    ((Sprite *)0x021C)->y = 0x78; ((Sprite *)0x021C)->x = 0x70;
    ((Sprite *)0x0220)->y = 0x78; ((Sprite *)0x0220)->x = 0x78;
    Player_LoadSprite0();

    PPU_Enable(
        /* flags */
          PPU_CTRL_F0_NAMETABLE_0
        | PPU_CTRL_F1_INC_1
        | PPU_CTRL_F2_FG_TABLE_0
        | PPU_CTRL_F3_BG_TABLE_0
        | PPU_CTRL_F4_SPRITE_SIZE_8X8
        | PPU_CTRL_F5_PPU_MASTER
        | PPU_CTRL_F6_NMI_ENABLE,
        /* mask */
          PPU_MASK_F0_COLOR
        | PPU_MASK_F1_BG_L8_SHOW
        | PPU_MASK_F2_FG_L8_SHOW
        | PPU_MASK_F3_BG_SHOW
        | PPU_MASK_F4_FG_SHOW);

//    k = 0x20;
//    for (i = 0; i < 4; ++i) {
//        PPU_ADDR_REG = 0x20;
//        PPU_ADDR_REG = k;
//        k += 0x20;
//        for (j = 0; j < 32; ++j) {
//            PPU_DATA_REG = 0x11;
//        }
//    }

    /* reset scroll */
    PPU_SetAddr(0x0000);
    WriteToRegister(PPU_SCRL, 0x00);
    WriteToRegister(PPU_SCRL, 0x00);

//    PPU_SetAddr(PPU_ADDR_NAMESPACE_START);
//    PPU_CTRL_REG = 32;
//    for (i = 0; i < 32; ++i) {
//        PPU_DATA_REG = 0x11;
//    }

LOOP:
    asm("NOP");
    goto LOOP;
}

static void PPU_TransferDMA(void)
{
    WriteToRegister(OAM_ADDR, 0x00);
    WriteToRegister(OAM_DMA,  0x02);
}

static void Joypad_Read()
{
    P1 = 1; // reset previously read input

    // start reading
    WriteToRegister(0x4016, 0x01); // set strobe bit (now buttons are start continuously reload)
    WriteToRegister(0x4016, 0x00); // clear strobe bit (now reloading stops and all buttons can be read from 0x4016)

    asm("READ_INPUT:");
    asm("LDA $4016"); // read button (only interested in state bit 0)
    asm("LSR a"); // shifts bits right, now CARRY = bit 0 of A
    asm("ROL %v", P1); // writes CARRY to bit 0 of P1
    asm("BCC READ_INPUT"); // branch on CARRY == 0
}

#define BUTTON_RIGHT  0x01
#define BUTTON_LEFT   0x02
#define BUTTON_DOWN   0x04
#define BUTTON_UP     0x08
#define BUTTON_START  0x10
#define BUTTON_SELECT 0x20
#define BUTTON_A      0x40
#define BUTTON_B      0x80

#pragma bss-name(push, "DATA")

//static const Sprite *sprites = (Sprite *)0x0200;

static u8 palette = 0;

#pragma bss-name(pop)

static void Player_HandleInput()
{
    if (P1) {
        if (P1 & BUTTON_SELECT) {

            if      (palette < 6)  { SetPalette0(); }
            else if (palette < 8)  { SetPalette1(); }
            else if (palette < 12) { SetPalette2(); }
            else                   { SetPalette3(); }

            palette++;
            if (palette > 16) {
                palette = 0;
            }

            PPU_SetAddr(0x0000);
        }
        if (P1 & BUTTON_A) {
            ShowDialog();
        }

        if (P1 & BUTTON_DOWN) {
            Player_CheckCollisionD();
            if (Player_collision_D == 0x81) {
                ((Sprite *)0x0204)->y += 1;
                ((Sprite *)0x0208)->y += 1;
                ((Sprite *)0x020C)->y += 1;
                ((Sprite *)0x0210)->y += 1;
                ((Sprite *)0x0214)->y += 1;
                ((Sprite *)0x0218)->y += 1;
                ((Sprite *)0x021C)->y += 1;
                ((Sprite *)0x0220)->y += 1;
            }
            Player_direction = 0;
            Player_OnDirectionChange();
        } else if (P1 & BUTTON_UP) {
            Player_CheckCollisionU();
            if (Player_collision_U == 0x81) {
                ((Sprite *)0x0204)->y -= 1;
                ((Sprite *)0x0208)->y -= 1;
                ((Sprite *)0x020C)->y -= 1;
                ((Sprite *)0x0210)->y -= 1;
                ((Sprite *)0x0214)->y -= 1;
                ((Sprite *)0x0218)->y -= 1;
                ((Sprite *)0x021C)->y -= 1;
                ((Sprite *)0x0220)->y -= 1;
            }
            Player_direction = 1;
            Player_OnDirectionChange();
        }
        if (P1 & BUTTON_RIGHT) {
            Player_CheckCollisionR();
            if (Player_collision_R == 0x81) {
                ((Sprite *)0x0204)->x += 1;
                ((Sprite *)0x0208)->x += 1;
                ((Sprite *)0x020C)->x += 1;
                ((Sprite *)0x0210)->x += 1;
                ((Sprite *)0x0214)->x += 1;
                ((Sprite *)0x0218)->x += 1;
                ((Sprite *)0x021C)->x += 1;
                ((Sprite *)0x0220)->x += 1;
            }
            Player_direction = 2;
            Player_OnDirectionChange();
        } else if (P1 & BUTTON_LEFT) {
            Player_CheckCollisionL();
            if (Player_collision_L == 0x81) {
                ((Sprite *)0x0204)->x -= 1;
                ((Sprite *)0x0208)->x -= 1;
                ((Sprite *)0x020C)->x -= 1;
                ((Sprite *)0x0210)->x -= 1;
                ((Sprite *)0x0214)->x -= 1;
                ((Sprite *)0x0218)->x -= 1;
                ((Sprite *)0x021C)->x -= 1;
                ((Sprite *)0x0220)->x -= 1;
            }
            Player_direction = 3;
            Player_OnDirectionChange();
        }
    }
}

static void NMI_Handler()
{
    Joypad_Read();
    Player_HandleInput();

    /* start OAM DMA transfer */
    PPU_TransferDMA();

    asm("RTI");
}

static void IRQ_Handler()
{
    asm("RTI");
}

#pragma code-name(pop)

/* Hardware vectors
 * ------------------------------------------------------------------------- */
#pragma data-name(push, "VECTORS")

struct { u16 NMI, RST, IRQ; } vectors = { (u16)NMI_Handler, (u16)RST_Handler, (u16)IRQ_Handler };

#pragma data-name(pop)