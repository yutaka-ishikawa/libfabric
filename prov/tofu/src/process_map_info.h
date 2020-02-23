/*
 * Copyright (c) 2008-2018      FUJITSU LIMITED.  All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#ifndef _TOFU_H_
#define _TOFU_H_

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <pthread.h>

#define TOFU_HW_VER                 3       /* TofuD */
#define TLIB_RANK_VERSION_MAJOR     3       /* メジャーチェンジごとに1加算 */
#define TLIB_RANK_VERSION_MINOR     0       /* マイナーチェンジごとに1加算 */

/*
座標軸のビットイメージ
31             24 23           16 15            8 7 6 5  2 1 0
 +---------------+---------------+---------------+---+----+---+
 |              x|              y|              z|  a|   b|  c|
 +---------------+---------------+---------------+---+----+---+
*/

typedef unsigned int    tlib_tofu6d;

typedef struct tlib_tofu_3d{
    unsigned int    x;
    unsigned int    y;
    unsigned int    z;
}                       tlib_tofu3d;

typedef tlib_tofu3d tlib_schd3d;

struct axis_assign{
    char logical_schd_x;
    char logical_schd_y;
    char logical_schd_z;
};

struct tlib_ranklist{                   /* ランクリスト */
    tlib_tofu6d         physical_addr;  /* 6次元物理座標 */
    tlib_tofu3d         virtual_addr;   /* N次元仮想座標 */
                                        /* 論理次元数が2次の場合 Z, 1次元の場合
                                           Y, Z は 0 を設定 */
};

/* この構造体は2020年現在は使う予定はない */
struct tlib_broken_link_info{
    unsigned int mode; /* 0: 故障 1: 縮退 */
    tlib_tofu6d addr1;
    tlib_tofu6d addr2;
};

/* オフセット <-> 実メモリポインタ変換 */
#define TLIB_OFFSET2PTR(offset_var) ((void*)((char*)&(offset_var) + (ulong)(offset_var)))
#define TLIB_PTR2OFFSET(real_maddr, base_maddr) (off_t)((ulong)(real_maddr) - (ulong)&(base_maddr))

/* 6次元座標から1軸の座標を取得する */
#define TLIB_GET_X(i)   ((tlib_tofu6d)(i) >> 24)
#define TLIB_GET_Y(i)   (((tlib_tofu6d)(i) >> 16) & 0x000000ff)
#define TLIB_GET_Z(i)   (((tlib_tofu6d)(i) >>  8) & 0x000000ff)
#define TLIB_GET_A(i)   (((tlib_tofu6d)(i) >>  6) & 0x00000003)
#define TLIB_GET_B(i)   (((tlib_tofu6d)(i) >>  2) & 0x0000000f)
#define TLIB_GET_C(i)   ((tlib_tofu6d)(i) & 0x00000003)

/* 6次元座標を一括設定する */
#define TLIB_SET_6D(x,y,z,a,b,c)    ((((x) & 0xff) << 24) | (((y) & 0xff) << 16) | (((z) & 0xff) << 8) | (((a) &0x03) << 6) | (((b) & 0x0f) << 2) | ((c) & 0x03))

/* 6次元座標の1軸の座標だけを入れ替える */
#define TLIB_SET_X(i,x) ((i) & ~0xff000000) | ((x) << 24)
#define TLIB_SET_Y(i,y) ((i) & ~0x00ff0000) | ((y) << 16)
#define TLIB_SET_Z(i,z) ((i) & ~0x0000ff00) | ((z) << 8)
#define TLIB_SET_A(i,a) ((i) & ~0x000000c0) | ((a) << 6)
#define TLIB_SET_B(i,b) ((i) & ~0x0000003c) | ((b) << 2)
#define TLIB_SET_C(i,c) ((i) & ~0x00000003) | (c)

/* 割り当て領域の粒度 */
#define TLIB_NODE_ALLOC_TOFU6D   0    /* Tofu単位を確保 */
#define TLIB_NODE_ALLOC_VIRT3D   1    /* ノード単位 */

/* 論理軸を構成する物理軸指定(オプション含む) */
#define TLIB_AXIS_X              0x20
#define TLIB_AXIS_Y              0x10
#define TLIB_AXIS_Z              0x08
#define TLIB_AXIS_A              0x04
#define TLIB_AXIS_B              0x02
#define TLIB_AXIS_C              0x01
#define TLIB_NO_TORUS            0x40 /* 非トーラス(論理1Dのみ設定可能） */
#define TLIB_MESH                0x80 /* メッシュ構造(未サポート) */
#define TLIB_TORUS               0x100 /* 仮想トーラス構造 */
/* ノード単位割り当てでジョブ形状が2次元の場合に論理軸を構成する仮想軸 */
#define TLIB_VIRT3D_X            (TLIB_AXIS_X | TLIB_AXIS_A)
#define TLIB_VIRT3D_Y            (TLIB_AXIS_Y | TLIB_AXIS_B)
#define TLIB_VIRT3D_Z            (TLIB_AXIS_Z | TLIB_AXIS_C)
#define TLIB_AXIS_MESH           0   /* 6次元Tofu物理軸の MESH, TORUS 定義 */
#define TLIB_AXIS_TORUS          1   /* system_torus にいずれかを設定 */
                                     /* TLIB_MESH, TLIB_AXIS_MESH の違いに
                                      * ついてご注意下さい */

#define TLIB_STACK_BAND          0   /* プロセス積み重ね方法定義 */
#define TLIB_STACK_CYCLIC        1   /* stack_sort にいずれかを設定 */

#define TLIB_STATIC_PROC_FREE    0   /* 静的プロセス生成無し(動的プロセス生成可) */
#define TLIB_STATIC_PROC_EXIST   1   /* 静的プロセス生成有り(動的プロセス生成不可) */

#define TLIB_ODD(n)  ((n) % 2 == 1)
#define TLIB_EVEN(n) ((n) % 2 == 0)

#define TLIB_CONV_SCHD3D_TO_PHYS6D(coord3d, coord6d)         \
    do{                                                      \
        coord6d   = TLIB_SET_6D(coord3d.x / 2,               \
        coord3d.y / 3,                                       \
        coord3d.z / 2,                                       \
        (coord3d.x % 4 == 0 || coord3d.x % 4 == 3) ? 0 : 1,  \
        (coord3d.y % 6 == 0 || coord3d.y % 6 == 5) ? 0 :     \
        (coord3d.y % 6 == 1 || coord3d.y % 6 == 4) ? 1 : 2,  \
        (coord3d.z % 4 == 0 || coord3d.z % 4 == 3) ? 0 : 1); \
    }while(0)

#define TLIB_CONV_PHYS6D_TO_SCHD3D(coord6d, coord3d) \
    do{                                                                 \
        coord3d.x = TLIB_EVEN(TLIB_GET_X(coord6d)) ? TLIB_GET_X(coord6d) * 2 + TLIB_GET_A(coord6d)    : \
                                                     TLIB_GET_X(coord6d) * 2 + 1 - TLIB_GET_A(coord6d); \
        coord3d.y = TLIB_EVEN(TLIB_GET_Y(coord6d)) ? TLIB_GET_Y(coord6d) * 3 + TLIB_GET_B(coord6d)    : \
                                                     TLIB_GET_Y(coord6d) * 3 + 2 - TLIB_GET_B(coord6d); \
        coord3d.z = TLIB_EVEN(TLIB_GET_Z(coord6d)) ? TLIB_GET_Z(coord6d) * 2 + TLIB_GET_C(coord6d)    : \
                                                     TLIB_GET_Z(coord6d) * 2 + 1 - TLIB_GET_C(coord6d); \
    }while(0)

/* 経路設定 */
#define     TLIB_PHYS_A     2       /* a 軸のサイズ: 2 */
#define     TLIB_PHYS_B     3       /* b 軸のサイズ: 3 */
#define     TLIB_PHYS_C     2       /* c 軸のサイズ: 2 */

struct tlib_axis_combination{       /* 論理軸を構成する物理軸の組合せ */
    char    logical_x;              /* X軸の組み合わせ: TLIB_AXIS_?の論理和 */
    char    logical_y;              /* Y軸の組み合わせ: TLIB_AXIS_?の論理和 */
                                    /* 次元数 < 2の場合, 無視 */
    char    logical_z;              /* Z軸の組み合わせ: TLIB_AXIS_?の論理和 */
                                    /* 次元数 < 3の場合, 無視 */
    char    rsrv[5];
};

struct tlib_err_list_detail {       /* ホストファイル, ノードアサインファイルに */
    unsigned int    err_cause;      /* 起因するエラー情報を保存する構造体 */
    int             err_index;      /* メンバはそれぞれ下記として使用する */
};                                  /* err_cause 原因判定コード(前半のdefine参照) */
                                    /* err_index: エラーが起きた配列要素番号
                                       座標範囲外, 故障の場合にはその要素番号の
                                       指定が誤りの原因
                                       重複指定の場合にはその要素番号以前に同じ座標
                                       指定があり, 指定可能数を超えた要素番号である
                                       配列要素数不足の場合には -1 を返す */

struct tlib_process_mapinfo{
    /* システム情報 */
    unsigned char   ver_major;          /* バージョンのメジャー番号 */
                                        /* 上位互換の変更管理, 自身よりも大きい場合NG */
                                        /* 小さい場合, 該当の互換処理を行う */
    unsigned char   ver_minor;          /* バージョンのマイナー番号 */
                                        /* 上下位互換の変更管理 */
                                        /* 自身より大きい場合, 自身の処理を行う */
                                        /* 小さい場合, 該当の互換処理を行う */
    tlib_tofu6d     system_size;        /* System全体のサイズ */
                                        /* 例 24,18,17,[2,3,2] */
                                        /* for 下方展開, []内は固定値 */
    tlib_tofu6d     system_torus;       /* SystemのTORUS/MESH */
                                        /* 0: MESH, 1: TORUS */
                                        /* 例 1,0,1,[0,1,0] */
                                        /* for 下方展開, []内は固定値 */

    /* 論理(仮想)座標生成の為の情報 */
    unsigned char   dimension;          /* 論理(仮想)次元数 */
    tlib_tofu3d     logical_size;       /* 論理(仮想)座標の各軸のサイズ  */
                                        /* 1, 2 次元では y, z or z は無視 */
    tlib_tofu6d     ref_addr;           /* 割り当て基準座標 (a, b, cは0, 0, 0固定)  */
    tlib_tofu6d     max_size;           /* 割り付けられた6次元の大きさ */
                                        /* (a, b, c は2, 3, 2 固定,
                                                すなわち Tofu 単位で割り当て */
                                        /* 物理軸が TORUS で無い場合は日付変更線(座標最大,
                                           0間)をまたぐ割り当て無きこと */
                                        /* サブシステム分割(ネットワーク分割)された場合に
                                           はサブシステムをまたぐ割り当て無きこと */
    unsigned int    broken_icc;         /* 6次元矩形の中に含まれるICC故障ノード数 */
                                        /* ノード割り当て不可, 通信経路としても使用不可 */
                                        /* ノード,   ICC ,  下記の故障モード */
                                        /*   x        x      o: 正常, x: 故障 */
                                        /*   o        x  */
    unsigned int    broken_node;        /* 6次元矩形の中に含まれる故障ノード数 */
                                        /* ICCは使用可能な故障ノード数(CPU, MEM不具合) */
                                        /* ノード割り当て不可, 通信経路としては使用可 */
                                        /* ノード,   ICC ,  下記の故障モード */
                                        /*   x        o  */
    off_t     offset_broken_node_list;  /* 故障ノードの位置情報メモリ領域への
                                           本変数のポインタからのオフセット
                                           TLIB_OFFSET2PTR(offset_var)を使用し実ポインタ
                                           に変換しアクセス
                                           実態は tofu6d 型配列
                                           領域サイズ (broken_icc + broken_node個) */
                                        /* 0～brokn_icc-1 : broken_icc に該当する故障の
                                                            6次元tofu座標 */
                                        /* broken_icc～
                                           broken_icc+broken_node-1:
                                                            broken_node に該当する故障の
                                                            6次元tofu座標
                                          ※6次元tofu座標は絶対座標 */
                                        /* メモリ領域の確保はPJM, PRM, PLEが行う */
    struct tlib_axis_combination        /* 論理軸を構成する物理軸の組合せ */
                            axis_image;

    unsigned int        universe_size;  /* ユーザーが指定する最大プロセス数 */
                                        /* proc_per_node * logical_sizeのメンバの積 */
    /* ノードアサインファイル用 IF (拡張機能で追加) */
    unsigned int num_node_assign_list;  /* ノードアサインファイルの読み込み行数
                                           基本的に logical_size.x, y, z の積
                                           論理 2D, 1D では x,y の積 または x
                                           ノードアサインファイルの有効行は上記値まで
                                           ノードアサインファイルを使用しない場合 0 を
                                           設定して下さい */
    unsigned int num_node_assign_bulk;  /* バルクジョブ, 任意ノード(ノード単位)割り当て
                                           において, 割り当てに使用する物理ノード数を指定
                                           バルクジョブ, 任意ノード割り当てを行わない
                                           場合 0 を指定してください */
                                        /* num_node_assign_list, num_node_assign_buld の
                                           の両方を非0に設定する事はできません(エラー) */
    off_t     offset_node_assign_list;  /* 論理座標 - 物理6D座標の関係を格納するメモリ
                                           領域への本変数ポインタからのオフセット値
                                           実態は tlib_ranklist 配列 (size は
                                           num_node_assign_list or _bulk 数分)
                                           ノードアサインファイルの使用ではジョブマネー
                                           ジャがファイルから読み込んだ値を設定
                                           バルクジョブでは, PLE が使用するノード数に
                                           応じてデータを生成
                                           ※6次元tofu座標は絶対座標 */

    /* プロセス生成に関する情報(静的, 動的共通) */
    unsigned char   max_proc_per_node;  /* 1ノード当たりの最大生成可能プロセス数 =
                                           CPUコア数(MP10 = 8, FX10 = 16) */
    unsigned char       proc_per_node;  /* ジョブ中の1ノード当たりの最大のプロセス数
                                           (1～N) N <= max_proc_per_node */

    /* 静的プロセス生成に関す情報(オート) */
    unsigned char          stack_sort;  /* プロセスのスタック方法(@1ノード複数プロセス)
                                           動的プロセス生成時は 0 を設定(MPI要件) */
                                        /* 0: band(ノード内優先),  */
                                        /* 1: cyclic(ノード外優先) */
    unsigned int           world_size;  /* 静的に起動されるMPI_COMM_WORLDのプロセス数 */
                                        /* 動的プロセス生成時は 0 を設定 */
                                        /* MPMDの場合, 静的に起動されるプログラムの
                                           プロセス数の和とする(正の整数値) */
    tlib_tofu3d     static_proc_shape;  /* 静的生成するプロセスの論理座標サイズ */
                                        /* 基準は(0,0,0) からとなる */
                                        /* logical_size を越えることは出来ない */
                                        /* 仮想トーラスは logical_size で構成される */
                                        /* ユーザ指定無き場合にはlogical_sizeを設定 */
                                        /* 空き領域には動的プロセス生成可能 */
                                        /* 動的プロセス生成時は(0,0,0)を設定(MPI要件) */
    unsigned char               order;  /* ランクの割り付けのオプション:
                                           0:     hostfileによる指定
                                           0以外: ランクの自動設定による指定(以下)
                                           動的プロセス生成時は 0 を設定(MPI要件)
                                          +-----------+-------+-------+-------+
                                          | dimension | 1次元 | 2次元 | 3次元 |
                                          +-----------+-------+-------+-------+
                                                      | 1(X)  | 1(XY) | 1(XYZ)|
                                                      +-------| 2(YX) | 2(XZY)|
                                                              +-------| 3(YXZ)|
                                                                      | 4(YZX)|
                                                                      | 5(ZXY)|
                                                                      | 6(ZYX)|
                                                                      +-------+*/
    /* hostfile の受け渡し */
    unsigned int         num_hostlist;  /* hostfileのhostの数(*hostlistのサイズ) */
                                        /* 動的プロセス生成時は 0 を設定(MPI要件) */
    off_t             offset_hostlist;  /* hostfile 座標情報メモリ領域への
                                           本変数のポインタからのオフセット
                                           TLIB_OFFSET2PTR(offset_var)を使用し実ポインタ
                                           に変換しアクセス
                                           実態は tofu3d 型配列
                                           領域サイズ (num_hostlist個) */
                                        /* メモリ領域の確保はPJM, PRM, PLEが行う */

    /* 動的プロセス生成(PLE -> Tofulib) */
    /* ランク配置情報(Tofulib -> PJM, PRM, PLE) */
    unsigned int             num_proc;  /* PLE -> Tofu への情報伝達時
                                           動的プロセス生成数(正の整数値)
                                           静的プロセス生成時は 0 を設定 */
                                        /* Tofu -> PJM, PRM, PLE時
                                           静的プロセス数 = world_size */
    off_t            offset_node_list;  /* 論理ID(RANK) と 論理座標, 物理座標のリストの
                                           メモリ領域への本変数ポインタからのオフセット
                                           TLIB_OFFSET2PTR(offset_var)を使用し実ポインタ
                                           に変換しアクセス
                                           実態は struct tlib_ranklist 構造体配列
                                           領域サイズ(num_proc 個)
                                           要素番号は論理ID(ランク)を示す */
                                        /* 動的プロセス生成時 PLE -> Tofulib へプロセス
                                           生成位置を指示(tlib_init, tlib_update時) */
                                        /* ランク配置生成時 Tofulib -> スケジューラ, PLE
                                           へ情報を返す(tlib_create_rankmap時) */
                                        /* データの伝達方向がいずれの場合においても
                                           node_listの実メモリ確保はPJM,PRM,PLE が行う*/

    /* 空き領域情報インタフェース(拡張機能で追加) */
    off_t    offset_logical_node_list;  /* 論理座標と物理座標の関係を一覧する為の配列
                                           順序は XYZ でサーチした順
                                           実態は struct tlib_ranklist 構造体配列
                                           メモリ確保は logical_size の x, y, z の積(3D),
                                           2, 1D では x, y の積または x となる
                                           ※6次元tofu座標は絶対座標 */
    off_t    offset_logical_node_proc;  /* 静的プロセスの生成有無を一覧する為の配列
                                           実態は unsigned char 型の配列(サイズ同上)
                                           論理座標(物理座標)の静的プロセス有無を設定
                                           有: TLIB_STATIC_PROC_EXIST=1,
                                           無: TLIB_STATIC_PROC_FREE=0 */

    struct tlib_err_list_detail         /* ホストファイル, ノードアサインファイルに */
                       err_list_detail; /* 起因するエラー情報を保存する構造体 */

    /* 以下はtlib_process_mapinfo version 2 で追加されたメンバ */
    size_t   mapinfo_size;              /* tlib_process_mapinfo 構造体自体のサイズ */
    size_t   total_size;                /* mapinfo_size + 故障情報等のサイズ */
    int allocation_mode;                /* 通常のノード単位割り当て(TLIB_NODE_ALLOC_VIRT3D: 1)か、
                                           Tofu 単位が確保されているか(TLIB_NODE_ALLOC_TOFU6D: 0)か
                                           を示す。*/
    tlib_schd3d system_size3d;          /* 仮想3次元空間の各軸のサイズ */
    tlib_schd3d system_torus3d;         /* 3次元空間の各軸がトーラスになっているかどうかを表す */
    tlib_schd3d ref_addr3d;             /* ノード単位割り当てにおけるプロセス割り当ての
                                           仮想3次元座標での基準点 */
    tlib_schd3d max_size3d;             /* ノード単位割り当てにおけるプロセス割り当ての
                                           仮想3次元座標でのサイズ */
    int axis_type;                      /* 論理軸のタイプを指定(仮想トーラス、メッシュ、ランダム)*/
    struct axis_assign axis_from_schd3d;/* 論理軸を構成する仮想3D軸 */
    int broken_link;                    /* 物理6次元領域の中に含まれる故障リンク数
                                           2020年現在は使う予定はなく、0に設定すること */
    off_t offset_broken_link_list;      /* 故障の位置・故障モード情報メモリ領域への本変数の
                                           ポインタからのオフセット */

    /* 以下はtlib_process_mapinfo version 3 で追加されたメンバ */
    off_t offset_nranklist;             /* 各プロセスが、ノード内で何番目に生成
                                           されるプロセスかを示す値のリスト。
                                           実態はint型配列。 */

};

struct tlib_process_mapinfo;

#endif /* _TOFU_H_ */
