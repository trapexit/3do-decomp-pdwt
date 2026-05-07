// SceneTiming.c - scene clock and audio timing.
#include "pdwt_timing.h"

#include "pdwt_input.h"

#define PDWT_DSP_SAMPLE_RATE 44100UL
#define PDWT_SCENE_SAMPLE_RATE 22050UL
#define PDWT_DSP_FRAMES_PER_SCENE_FRAME (PDWT_DSP_SAMPLE_RATE / PDWT_SCENE_SAMPLE_RATE)
#define PDWT_SC01_AUDIO_START_OFFSET_DSP_FRAMES 0UL
#define PDWT_SC01_AUDIO_END_DSP_FRAMES 4044292UL
#define PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES 0UL
#define PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES 0UL
#define PDWT_SC03_AUDIO_END_DSP_FRAMES 3201608UL
#define PDWT_SC04_AUDIO_END_DSP_FRAMES 4431872UL
#define PDWT_SC06_REFERENCE_END_DSP_FRAMES 2788296UL
#define PDWT_SC07_AUDIO_END_DSP_FRAMES 4421128UL
#define PDWT_SC08_AUDIO_END_DSP_FRAMES 3383296UL
#define PDWT_SC09_REFERENCE_END_DSP_FRAMES 9284050UL
#define PDWT_SC10_AUDIO_END_DSP_FRAMES 3020800UL
#define PDWT_SC11_AUDIO_END_DSP_FRAMES 7591582UL
#define PDWT_SC12_AUDIO_END_DSP_FRAMES 18724864UL
#define PDWT_SC13_AUDIO_END_DSP_FRAMES 4257792UL

// AudioTime ticks span GetAudioDuration() DSP frames. The default is 184,
// not exactly 1/240 second.

typedef struct SceneAudioLength
{
  int32 scene;
  uint32 sampleFrames;
} SceneAudioLength;

typedef struct SceneTimingTargets
{
  int32 scene;
  const uint32 *dspFrames;
  uint32 count;
  uint32 audioStartOffsetDSPFrames;
  uint32 completionDSPFrames;
} SceneTimingTargets;

// Exact AIFF sample-frame counts, converted to AudioTime at scene startup.
static const SceneAudioLength gSceneAudioLengths[] =
{
  {1, 2022146UL}, {2, 4045824UL}, {3, 1600804UL}, {4, 2215936UL}, {6, 1524399UL},
  {7, 2210564UL}, {8, 1691648UL}, {9, 4853760UL}, {10, 1510400UL}, {11, 3795791UL},
  {12, 9362432UL}, {13, 2128896UL}, {15, 3133440UL}, {16, 2317383UL}, {17, 2454528UL},
  {18, 3222379UL}, {20, 1925120UL}, {21, 1840405UL}, {22, 4706721UL}, {23, 4057088UL},
  {24, 3148941UL}, {25, 5085780UL}, {26, 7190268UL}, {27, 3740155UL}, {28, 2402690UL},
  {29, 1177978UL}, {30, 3190481UL}, {31, 5353570UL}, {32, 2192384UL}
};

// Cumulative SC01 cut points from the DVD reference, scaled by the exact
// 3DO-to-DVD audio-length ratio (2022146 / 4079780). Values are 44.1 kHz
// DSP frames so they remain accurate if AudioTime's tick duration changes.
// Direct sound-file playback starts immediately before the scene clock.
static const uint32 gSC01TargetDSPFrames[] =
{
  350081UL,  459481UL,  571789UL,  701255UL,  791332UL,  922613UL,  1008297UL, 1141413UL,
  1230026UL, 1361307UL, 1428761UL, 1515188UL, 1580828UL, 1713202UL, 1779585UL, 1867105UL,
  1975763UL, 2043239UL, 2129666UL, 2197121UL, 2284641UL, 2349560UL, 2437080UL, 2502720UL,
  2634722UL, 2724078UL, 2810855UL, 2920998UL, 3032213UL, 3117919UL, 3208346UL, 3294773UL,
  3405267UL, 3492787UL, 3559149UL, 3647761UL, 3755348UL, 3890278UL, 3999678UL
};

// Reference cut targets relative to the first source AIFF sample.
static const uint32 gSC02TargetDSPFrames[] =
{
  // Start shakeeffect() 16 VBLs before its fourth visible cut.
  344139UL, 1051900UL, 1315309UL, 1505868UL, 1617143UL, 1848698UL,
  2115062UL, 2291639UL, 2688936UL, 3128877UL, 3489395UL, 3796199UL,
  4153012UL, 4594453UL, 4885072UL, 5394956UL, 6051208UL, 6675135UL,
  7073887UL, 7382896UL, 7599956UL, 7958974UL, 8048762UL, 8125981UL
};

static const uint32 gSC03TargetDSPFrames[] =
{
  96851UL,  319777UL, 588302UL, 983394UL, 1468979UL,
  1912625UL, 2137050UL, 2490202UL, 2933099UL, 3150159UL
};

static const uint32 gSC04TargetDSPFrames[] =
{
  261009UL,  568519UL,  837088UL,  1079858UL, 1565443UL, 1699375UL,
  1831807UL, 1966444UL, 2098877UL, 2231309UL, 2367402UL, 2499834UL,
  2635927UL, 2854442UL, 2989079UL, 3163451UL, 3343732UL, 3564452UL,
  3788833UL, 3965409UL, 4138325UL, 4318562UL, 4431872UL
};

static const uint32 gSC06TargetDSPFrames[] =
{
  97623UL,   134402UL,  183706UL,  220486UL,  268290UL,  307318UL,
  356578UL,  402927UL,  441206UL,  489010UL,  539064UL,  575843UL,
  614122UL,  650902UL,  700911UL,  748759UL,  798019UL,  844368UL,
  882647UL,  932656UL,  981960UL,  1029809UL, 1077613UL, 1125462UL,
  1173266UL, 1221070UL, 1272579UL, 1322633UL, 1368232UL, 1418242UL,
  1471250UL, 1517599UL, 1565403UL, 1614707UL, 1655191UL, 1690471UL,
  1739775UL, 1797149UL, 1846452UL, 1894301UL, 1942105UL, 1993614UL,
  2037758UL, 2084107UL, 2131912UL, 2176056UL, 2223904UL, 2271709UL,
  2323218UL, 2371022UL, 2418870UL, 2466675UL, 2720514UL
};

static const uint32 gSC07TargetDSPFrames[] =
{
  248136UL,  471062UL,  649138UL,  822009UL,  1002290UL,
  1270815UL, 1490080UL, 1682842UL, 1817479UL, 2277310UL,
  2598843UL, 2954200UL, 3305148UL, 3702445UL, 4142387UL
};

static const uint32 gSC08TargetDSPFrames[] =
{
  205756UL, 417657UL,  1154876UL, 1260099UL, 1403556UL, 1609547UL,
  1705200UL, 1916351UL, 2107657UL, 2569692UL, 3060437UL
};

static const uint32 gSC09TargetDSPFrames[] =
{
  489010UL,  772309UL,  1839088UL, 2194446UL, 3509243UL, 4784262UL,
  5032192UL, 5190379UL, 5889320UL, 6268227UL, 6855331UL, 7419634UL,
  7721322UL, 8153899UL, 8504847UL, 8827130UL, 9161893UL
};

static const uint32 gSC10TargetDSPFrames[] =
{
  301409UL, 550089UL, 881192UL, 1666216UL, 2306283UL, 2955964UL
};

static const uint32 gSC11TargetDSPFrames[] =
{
  658266UL, 1073953UL, 1830268UL, 2102497UL, 2396776UL, 2680031UL, 2909615UL,
  3472420UL, 3994828UL, 4618711UL, 5046922UL, 6389635UL, 7541041UL
};

static const uint32 gSC12TargetDSPFrames[] =
{
  290384UL,   485350UL,   728165UL,   1283604UL,  1857478UL,  2504954UL,
  4508328UL,  4789422UL,  5126346UL,  5355930UL,  5672304UL,  5961424UL,
  6252043UL,  6834736UL,  8317246UL,  8486457UL,  8702018UL,  9018391UL,
  9409823UL,  10002086UL, 10798885UL, 11122623UL, 11558155UL, 12388072UL,
  12572014UL, 12818488UL, 13125292UL, 13384997UL, 13570393UL, 14000809UL,
  14330413UL, 14596733UL, 14747555UL, 15181675UL, 15376641UL, 15496549UL,
  15669465UL, 15901211UL, 16132956UL, 16517023UL, 17020998UL, 17479373UL,
  18591796UL
};

static const uint32 gSC13TargetDSPFrames[] =
{
  455935UL,  689886UL,  1315268UL, 2187125UL, 2308488UL, 2497589UL,
  2731540UL, 3367947UL, 3590166UL, 3849871UL, 4055862UL
};

static const uint32 gSC15TargetDSPFrames[] =
{
  266835UL,  745055UL,  838503UL,  1335113UL, 1469750UL, 1938445UL,
  2098837UL, 3209760UL, 3454030UL, 3880785UL, 3991124UL, 4670220UL,
  4969659UL, 5347066UL, 5595041UL, 6023957UL, 6266880UL
};

static const uint32 gSC16TargetDSPFrames[] =
{
  327163UL, 812748UL, 1232140UL, 1636757UL, 1758164UL,
  2081902UL, 3420955UL, 3932250UL, 4634766UL
};

static const uint32 gSC17TargetDSPFrames[] =
{
  176341UL,  573638UL,  912062UL,  1368232UL, 1772894UL, 2269504UL, 2622657UL,
  3126631UL, 3420955UL, 3774108UL, 4061022UL, 4329547UL, 4679040UL, 4909056UL
};

static const uint32 gSC18TargetDSPFrames[] =
{
  275655UL,  477985UL,  592028UL,  849528UL,  1055563UL, 1261554UL, 1427106UL,
  1611047UL, 1897961UL, 2000979UL, 2332038UL, 2534368UL, 2898546UL, 3354717UL,
  4002149UL, 4598116UL, 4958634UL, 5414760UL, 5922439UL, 6444758UL
};

static const uint32 gSC20TargetDSPFrames[] =
{
  308774UL,  669291UL,  1666216UL, 1879572UL, 2059808UL, 2383546UL,
  2648411UL, 2744064UL, 2920640UL, 3104581UL, 3402521UL, 3850240UL
};

static const uint32 gSC21TargetDSPFrames[] =
{
  146927UL,  422816UL,  588368UL,  790698UL,  871622UL,  1276284UL,
  1552173UL, 1706655UL, 1769189UL, 2118682UL, 2258479UL, 2699920UL,
  2747724UL, 2824987UL, 2939030UL, 3008928UL, 3078827UL, 3211259UL,
  3284818UL, 3413590UL, 3479784UL, 3680810UL
};

static const uint32 gSC22TargetDSPFrames[] =
{
  235215UL,  444866UL,  581003UL,  1338817UL, 1449156UL, 1728750UL,
  1835428UL, 2140776UL, 2339402UL, 2552758UL, 2762454UL, 2968445UL,
  3167115UL, 3376811UL, 3590166UL, 3991124UL, 4215549UL, 4620166UL,
  4837226UL, 5109456UL, 5348566UL, 5731133UL, 5933464UL, 6268227UL,
  6470558UL, 6643474UL, 6753812UL, 6930389UL, 7162134UL, 7493237UL,
  7938338UL, 8324610UL, 9413442UL
};

static const uint32 gSC23TargetDSPFrames[] =
{
  189571UL,  323503UL,  588368UL,  853232UL,  1132782UL, 1249030UL,
  1403556UL, 1848657UL, 2062013UL, 2339402UL, 2578512UL, 2735244UL,
  2920640UL, 3045708UL, 3273793UL, 3504083UL, 3713735UL, 3813092UL,
  3960210UL, 4222869UL, 4517193UL, 5089566UL, 5641390UL, 6271888UL,
  6617720UL, 6827371UL, 7147449UL, 7690408UL, 8114176UL
};

static const uint32 gSC24TargetDSPFrames[] =
{
  56780UL,   189212UL,  321645UL,  446712UL,  512950UL,  638018UL,  704212UL,
  832984UL,  961756UL,  1027950UL, 1171408UL, 1219256UL, 1347984UL, 1476756UL,
  1605528UL, 1734256UL, 1863028UL, 1991756UL, 2054290UL, 2120528UL, 2249256UL,
  2378028UL, 2506800UL, 2565629UL, 2635528UL, 2734841UL, 2867273UL, 2955561UL,
  3025460UL, 3154232UL, 3279299UL, 3411732UL, 3536799UL, 3603037UL, 3669231UL,
  3798003UL, 4129062UL, 4261494UL, 4401292UL, 4519038UL, 4622012UL, 4684546UL,
  4813318UL, 4942046UL, 5070818UL, 5199590UL, 5320953UL, 5457090UL, 5585817UL,
  5828588UL
};

static const uint32 gSC25TargetDSPFrames[] =
{
  117468UL,  235215UL,  397062UL,  477985UL,  555249UL,  650902UL,
  728165UL,  890012UL,  1059223UL, 1143851UL, 1228435UL, 1349842UL,
  1460181UL, 1555833UL, 1728750UL, 1806013UL, 1890597UL, 2063513UL,
  2140776UL, 2225360UL, 2398276UL, 2508614UL, 2626361UL, 2729334UL,
  2836012UL, 2942690UL, 3152386UL, 3928590UL, 4072047UL, 4340616UL,
  5194039UL, 5429489UL, 5572947UL, 5999702UL, 6323396UL, 6455829UL,
  6602990UL, 6842101UL, 7136380UL, 7335050UL, 7938338UL, 8203203UL,
  8567381UL, 9321535UL, 10171560UL
};

static const uint32 gSC26TargetDSPFrames[] =
{
  145057UL,   336363UL,   851363UL,   1064763UL,  1758544UL,  2269883UL,
  2659816UL,  2880536UL,  3294018UL,  3580976UL,  3809017UL,  3948814UL,
  4076087UL,  4190130UL,  4401280UL,  4603611UL,  5039937UL,  5536547UL,
  5913249UL,  6498147UL,  7645894UL,  8229337UL,  8880473UL,  9064370UL,
  9259336UL,  9730236UL,  9970802UL,  10154743UL, 10403423UL, 10568974UL,
  10892668UL, 11223771UL, 12297959UL, 12788659UL, 14380536UL
};

static const uint32 gSC27TargetDSPFrames[] =
{
  371308UL,  675156UL,  1620572UL, 2361496UL, 3721099UL, 4108827UL, 4660650UL,
  4877710UL, 4982889UL, 5225703UL, 6038687UL, 6327101UL, 6875220UL, 7480310UL
};

static const uint32 gSC28TargetDSPFrames[] =
{
  220486UL,  393402UL,  601598UL,  805384UL,  881192UL,
  1073953UL, 1232140UL, 1440336UL, 1922260UL, 2243749UL,
  3202439UL, 3849871UL, 4016878UL, 4359006UL, 4805380UL
};

static const uint32 gSC29TargetDSPFrames[] =
{
  303614UL, 592028UL, 842163UL, 1279944UL, 2355956UL
};

static const uint32 gSC30TargetDSPFrames[] =
{
  312434UL,  455935UL,  628807UL,  849528UL,  1099707UL, 1329953UL, 1989910UL,
  2562328UL, 2900751UL, 3509243UL, 4395785UL, 4607686UL, 4736414UL, 4899760UL,
  5142575UL, 5398575UL, 5623000UL, 5841516UL, 6007023UL, 6183599UL, 6380962UL
};

static const uint32 gSC31TargetDSPFrames[] =
{
  227850UL,  402927UL,  518469UL,  1193861UL, 2135616UL, 2474040UL,
  3016293UL, 3571777UL, 4059567UL, 5060152UL, 5435355UL, 6054871UL,
  7209983UL, 7813271UL, 8225297UL, 9431873UL, 10707140UL
};

static const uint32 gSC32TargetDSPFrames[] =
{
  204301UL,  459596UL,  631012UL,  888512UL,  1054064UL, 1232140UL,
  1407216UL, 1577928UL, 1739775UL, 1916351UL, 2089267UL, 2264344UL,
  2435055UL, 2606472UL, 2773479UL, 2950055UL, 3122971UL, 3292183UL,
  3459939UL, 3626946UL, 3814547UL, 4158880UL, 4384768UL
};

static const SceneTimingTargets gSceneTimingTargets[] =
{
  {1,
   gSC01TargetDSPFrames,
   sizeof(gSC01TargetDSPFrames) / sizeof(gSC01TargetDSPFrames[0]),
   PDWT_SC01_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC01_AUDIO_END_DSP_FRAMES},
  {2,
   gSC02TargetDSPFrames,
   sizeof(gSC02TargetDSPFrames) / sizeof(gSC02TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {3,
   gSC03TargetDSPFrames,
   sizeof(gSC03TargetDSPFrames) / sizeof(gSC03TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC03_AUDIO_END_DSP_FRAMES},
  {4,
   gSC04TargetDSPFrames,
   sizeof(gSC04TargetDSPFrames) / sizeof(gSC04TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC04_AUDIO_END_DSP_FRAMES},
  {6,
   gSC06TargetDSPFrames,
   sizeof(gSC06TargetDSPFrames) / sizeof(gSC06TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC06_REFERENCE_END_DSP_FRAMES},
  {7,
   gSC07TargetDSPFrames,
   sizeof(gSC07TargetDSPFrames) / sizeof(gSC07TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC07_AUDIO_END_DSP_FRAMES},
  {8,
   gSC08TargetDSPFrames,
   sizeof(gSC08TargetDSPFrames) / sizeof(gSC08TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC08_AUDIO_END_DSP_FRAMES},
  {9,
   gSC09TargetDSPFrames,
   sizeof(gSC09TargetDSPFrames) / sizeof(gSC09TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC09_REFERENCE_END_DSP_FRAMES},
  {10,
   gSC10TargetDSPFrames,
   sizeof(gSC10TargetDSPFrames) / sizeof(gSC10TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC10_AUDIO_END_DSP_FRAMES},
  {11,
   gSC11TargetDSPFrames,
   sizeof(gSC11TargetDSPFrames) / sizeof(gSC11TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC11_AUDIO_END_DSP_FRAMES},
  {12,
   gSC12TargetDSPFrames,
   sizeof(gSC12TargetDSPFrames) / sizeof(gSC12TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC12_AUDIO_END_DSP_FRAMES},
  {13,
   gSC13TargetDSPFrames,
   sizeof(gSC13TargetDSPFrames) / sizeof(gSC13TargetDSPFrames[0]),
   PDWT_MIKE_AUDIO_START_OFFSET_DSP_FRAMES,
   PDWT_SC13_AUDIO_END_DSP_FRAMES},
  {15,
   gSC15TargetDSPFrames,
   sizeof(gSC15TargetDSPFrames) / sizeof(gSC15TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {16,
   gSC16TargetDSPFrames,
   sizeof(gSC16TargetDSPFrames) / sizeof(gSC16TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {17,
   gSC17TargetDSPFrames,
   sizeof(gSC17TargetDSPFrames) / sizeof(gSC17TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {18,
   gSC18TargetDSPFrames,
   sizeof(gSC18TargetDSPFrames) / sizeof(gSC18TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {20,
   gSC20TargetDSPFrames,
   sizeof(gSC20TargetDSPFrames) / sizeof(gSC20TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {21,
   gSC21TargetDSPFrames,
   sizeof(gSC21TargetDSPFrames) / sizeof(gSC21TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {22,
   gSC22TargetDSPFrames,
   sizeof(gSC22TargetDSPFrames) / sizeof(gSC22TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {23,
   gSC23TargetDSPFrames,
   sizeof(gSC23TargetDSPFrames) / sizeof(gSC23TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {24,
   gSC24TargetDSPFrames,
   sizeof(gSC24TargetDSPFrames) / sizeof(gSC24TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {25,
   gSC25TargetDSPFrames,
   sizeof(gSC25TargetDSPFrames) / sizeof(gSC25TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {26,
   gSC26TargetDSPFrames,
   sizeof(gSC26TargetDSPFrames) / sizeof(gSC26TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {27,
   gSC27TargetDSPFrames,
   sizeof(gSC27TargetDSPFrames) / sizeof(gSC27TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {28,
   gSC28TargetDSPFrames,
   sizeof(gSC28TargetDSPFrames) / sizeof(gSC28TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {29,
   gSC29TargetDSPFrames,
   sizeof(gSC29TargetDSPFrames) / sizeof(gSC29TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {30,
   gSC30TargetDSPFrames,
   sizeof(gSC30TargetDSPFrames) / sizeof(gSC30TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {31,
   gSC31TargetDSPFrames,
   sizeof(gSC31TargetDSPFrames) / sizeof(gSC31TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0},
  {32,
   gSC32TargetDSPFrames,
   sizeof(gSC32TargetDSPFrames) / sizeof(gSC32TargetDSPFrames[0]),
   PDWT_TUNTEST_AUDIO_START_OFFSET_DSP_FRAMES,
   0}
};

static
int32
_scene_number(const uint8 *sceneName_);


static
const
uint32 *
_scene_timing_targets(const uint8 *sceneName_,
                      uint32      *count_,
                      uint32      *audioStartOffset_,
                      uint32      *completionDSPFrames_);


static
uint32
_dsp_frames_to_audio_ticks(uint32 dspFrames_,
                           uint32 audioTickFrames_);


static
uint32
_scene_audio_ticks(const uint8 *sceneName_,
                   uint32       audioTickFrames_);


static
int32
_wait_for_audio_time(Item      vblIOReq_,
                     AudioTime target_,
                     uint32    stopMask_);


static
void
_advance_scene_clock(PDWTSceneClock *clock_,
                     uint32          fields_);
static
int32
_scene_number(const uint8 *sceneName_)
{
  int32 scene;

  if(sceneName_ == 0 || sceneName_[0] != 's' || sceneName_[1] != 'c' ||
     sceneName_[2] < '0' || sceneName_[2] > '9' || sceneName_[3] < '0' || sceneName_[3] > '9')
    {
      return -1;
    }

  scene = ((int32)sceneName_[2] - (int32)'0') * 10;
  scene += (int32)sceneName_[3] - (int32)'0';
  return scene;
}


static
const
uint32 *
_scene_timing_targets(const uint8 *sceneName_,
                      uint32      *count_,
                      uint32      *audioStartOffset_,
                      uint32      *completionDSPFrames_)
{
  int32 scene;
  uint32 i;

  *count_               = 0;
  *audioStartOffset_    = 0;
  *completionDSPFrames_ = 0;
  scene                 = _scene_number(sceneName_);
  for(i = 0; i < sizeof(gSceneTimingTargets) / sizeof(gSceneTimingTargets[0]); i++)
    {
      if(gSceneTimingTargets[i].scene == scene)
        {
          *count_               = gSceneTimingTargets[i].count;
          *audioStartOffset_    = gSceneTimingTargets[i].audioStartOffsetDSPFrames;
          *completionDSPFrames_ = gSceneTimingTargets[i].completionDSPFrames;
          return gSceneTimingTargets[i].dspFrames;
        }
    }
  return NULL;
}


static
uint32
_dsp_frames_to_audio_ticks(uint32 dspFrames_,
                           uint32 audioTickFrames_)
{
  if(audioTickFrames_ == 0)
    {
      return 0;
    }
  return (dspFrames_ + (audioTickFrames_ / 2)) / audioTickFrames_;
}


static
uint32
_scene_audio_ticks(const uint8 *sceneName_,
                   uint32       audioTickFrames_)
{
  int32 scene;
  uint32 dspFrames;
  uint32 i;
  uint32 sampleFrames;

  if(audioTickFrames_ == 0)
    {
      return 0;
    }

  scene = _scene_number(sceneName_);
  for(i = 0; i < sizeof(gSceneAudioLengths) / sizeof(gSceneAudioLengths[0]); i++)
    {
      if(gSceneAudioLengths[i].scene == scene)
        {
          sampleFrames = gSceneAudioLengths[i].sampleFrames;
          dspFrames = sampleFrames * PDWT_DSP_FRAMES_PER_SCENE_FRAME;
          return _dsp_frames_to_audio_ticks(dspFrames, audioTickFrames_);
        }
    }
  return 0;
}


void
PDWTSceneClockStart(PDWTSceneClock *clock_,
                    const uint8    *sceneName_,
                    uint32          totalFields_)
{
  uint32 completionDSPFrames;

  if(clock_ == NULL)
    {
      return;
    }

  clock_->audioTickFrames = GetAudioDuration();
  clock_->startTime       = GetAudioTime();
  clock_->pauseTime       = 0;
  clock_->totalFields     = totalFields_;
  clock_->elapsedFields   = 0;
  clock_->targetDSPFrames = _scene_timing_targets(
    sceneName_,
    &clock_->targetCount,
    &clock_->targetOffsetDSPFrames,
    &completionDSPFrames);
  clock_->targetIndex     = 0;
  if(clock_->targetCount != 0)
    {
      if(completionDSPFrames == 0)
        {
          completionDSPFrames = clock_->targetDSPFrames[clock_->targetCount - 1];
        }
      clock_->audioTicks = _dsp_frames_to_audio_ticks(
        completionDSPFrames + clock_->targetOffsetDSPFrames,
        clock_->audioTickFrames);
    }
  else
    {
      clock_->audioTicks = _scene_audio_ticks(sceneName_, clock_->audioTickFrames);
    }
  clock_->elapsedTicks  = 0;
  clock_->tickRemainder = 0;
  clock_->active        = totalFields_ != 0 && clock_->audioTicks != 0;
  clock_->paused        = 0;

}


void
PDWTSceneClockPause(PDWTSceneClock *clock_)
{
  if(clock_ == NULL || !clock_->active || clock_->paused)
    {
      return;
    }

  clock_->pauseTime = GetAudioTime();
  clock_->paused    = 1;
}


void
PDWTSceneClockResume(PDWTSceneClock *clock_)
{
  AudioTime resumeTime;

  if(clock_ == NULL || !clock_->active || !clock_->paused)
    {
      return;
    }

  resumeTime       = GetAudioTime();
  clock_->startTime += resumeTime - clock_->pauseTime;
  clock_->pauseTime = 0;
  clock_->paused    = 0;
}


int32
PDWTSceneClockIsCalibrated(const PDWTSceneClock *clock_)
{
  return clock_ != NULL && clock_->targetDSPFrames != 0;
}


static
int32
_wait_for_audio_time(Item      vblIOReq_,
                     AudioTime target_,
                     uint32    stopMask_)
{
  uint32 buttons;
  int32 result;
  int32 waited;

  if(vblIOReq_ <= 0)
    {
      return -1;
    }

  waited = 0;
  while(!AudioTimeLaterThanOrEqual(GetAudioTime(), target_))
    {
      result = WaitForJoypad(vblIOReq_, 1, stopMask_);
      if(result != PDWT_INPUT_COMPLETE)
        {
          return result;
        }
      waited = 1;
    }

  if(!waited)
    {
      buttons = GetJoypad();
      if(controlPadLastError < 0)
        {
          return controlPadLastError;
        }
      if((buttons & stopMask_) != 0)
        {
          return PDWT_INPUT_STOP;
        }
    }
  return PDWT_INPUT_COMPLETE;
}


static
void
_advance_scene_clock(PDWTSceneClock *clock_,
                     uint32          fields_)
{
  uint32 baseTicks;
  uint32 extraTicks;
  uint32 field;

  if(fields_ > clock_->totalFields - clock_->elapsedFields)
    {
      fields_ = clock_->totalFields - clock_->elapsedFields;
    }

  clock_->elapsedFields += fields_;
  if(clock_->targetDSPFrames != 0)
    {
      if(clock_->targetIndex < clock_->targetCount)
        {
          clock_->elapsedTicks = _dsp_frames_to_audio_ticks(
            clock_->targetDSPFrames[clock_->targetIndex] + clock_->targetOffsetDSPFrames,
            clock_->audioTickFrames);
          clock_->targetIndex++;
        }
      return;
    }

  baseTicks  = clock_->audioTicks / clock_->totalFields;
  extraTicks = clock_->audioTicks % clock_->totalFields;
  for(field = 0; field < fields_; field++)
    {
      clock_->elapsedTicks += baseTicks;
      clock_->tickRemainder += extraTicks;
      if(clock_->tickRemainder >= clock_->totalFields)
        {
          clock_->elapsedTicks++;
          clock_->tickRemainder -= clock_->totalFields;
        }
    }
}


void
PDWTSceneClockAdvance(PDWTSceneClock *clock_,
                      int32           fields_)
{
  if(clock_ == NULL || !clock_->active || clock_->paused || fields_ <= 0)
    {
      return;
    }

  _advance_scene_clock(clock_, (uint32)fields_);
}


int32
PDWTSceneClockReached(const PDWTSceneClock *clock_)
{
  AudioTime target;

  if(clock_ == NULL || !clock_->active || clock_->paused)
    {
      return 0;
    }

  target = clock_->startTime + clock_->elapsedTicks;
  return AudioTimeLaterThanOrEqual(GetAudioTime(), target);
}


int32
PDWTSceneClockEndReached(const PDWTSceneClock *clock_)
{
  AudioTime target;

  if(clock_ == NULL || !clock_->active || clock_->paused)
    {
      return 0;
    }

  target = clock_->startTime + clock_->audioTicks;
  return AudioTimeLaterThanOrEqual(GetAudioTime(), target);
}


int32
PDWTSceneClockWait(PDWTSceneClock *clock_,
                   Item            vblIOReq_,
                   int32           fields_,
                   uint32          stopMask_)
{
  AudioTime target;
  int32 result;

  if(clock_ == NULL || !clock_->active)
    {
      return WaitForJoypad(vblIOReq_, fields_, stopMask_);
    }
  if(fields_ < 0)
    {
      return -1;
    }

  PDWTSceneClockAdvance(clock_, fields_);
  target = clock_->startTime + clock_->elapsedTicks;
  result = _wait_for_audio_time(vblIOReq_, target, stopMask_);
  if(result != PDWT_INPUT_COMPLETE)
    {
      return result;
    }

  return PDWT_INPUT_COMPLETE;
}


int32
PDWTSceneClockWaitForEnd(PDWTSceneClock *clock_,
                         Item            vblIOReq_,
                         uint32          stopMask_)
{
  AudioTime target;
  int32 result;

  if(clock_ == NULL || !clock_->active)
    {
      return PDWT_INPUT_COMPLETE;
    }

  target = clock_->startTime + clock_->audioTicks;
  result = _wait_for_audio_time(vblIOReq_, target, stopMask_);
  return result;
}
