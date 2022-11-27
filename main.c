#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fnmatch.h>
#include <string.h>

#include "debugmalloc.h"

// Adat struktúra, ami alapján eltároljuk a bevitt listaat
typedef struct AdatMezo
{
    char vezeteknev[255 + 1];
    char keresztnev[255 + 1];
    char foglalkozas[255 + 1];
    char varos[255 + 1];
    char telefonszam[11 + 1];
} AdatMezo;

// Láncolt lista, az listanak tárolásához
typedef struct ListaElem
{
    AdatMezo adat[1];
    struct ListaElem *kov;
} ListaElem;

typedef struct KeresesiListaElem
{
    int index;
    char *query;
    struct KeresesiListaElem *kov;
} KeresesiListaElem;

typedef struct DinTomb
{
    char *tomb;
    int meret;
} DinTomb;

static bool dianamikus_lefoglalas(DinTomb *dt, int meret);
static bool dintomb_atmeretez(DinTomb *dt, int ujmeret);
static void lista_felszabaditas(ListaElem *lista);
static void keresesi_lista_felszabaditas(KeresesiListaElem *lista);
static void lista_kiiras(ListaElem *lista);
static bool megfelelo_e(int valaszt, int *lehetosegek, int szam);
static void csere(ListaElem *a, ListaElem *b);
static void buborek_rendezes(ListaElem *lista);
static char *string_beolvasas(bool wildcard);
static bool forma_helyes_bemenet(char *szoveg, int max_char);
static ListaElem *beolvasas_kezzel(void);
static ListaElem *beolvasas_fajlbol(char *fajlnev);
static char *parositas(int valasztas);
static char *get_query(KeresesiListaElem *query, int tipus);
static ListaElem *kereses_a_listaban(ListaElem *lista, KeresesiListaElem *query, int *tipus, int tipus_db);
static void kilepes(ListaElem *lista);
void fomenu(ListaElem *lista);
void menu_3(ListaElem *lista);
void adatok_beolvasasa(void);

// Dinamikus tömb lefoglalás
static bool dianamikus_lefoglalas(DinTomb *dt, int meret)
{
    dt->meret = meret;
    dt->tomb = (char *)malloc(meret * sizeof(char));
    return dt->tomb != NULL;
}

// Dinamikus tömb átméretezés
static bool dintomb_atmeretez(DinTomb *dt, int ujmeret)
{
    char *ujtomb = (char *)malloc(ujmeret * sizeof(char));
    if (ujtomb == NULL)
        return false;
    int min = ujmeret < dt->meret ? ujmeret : dt->meret;
    for (int i = 0; i < min; ++i)
        ujtomb[i] = dt->tomb[i];
    free(dt->tomb);
    dt->tomb = ujtomb; /* ! */
    dt->meret = ujmeret;
    return true;
}

// Lista memóriaterület felszabadítása
static void lista_felszabaditas(ListaElem *lista)
{
    ListaElem *mozgo = lista;
    while (mozgo != NULL)
    {
        ListaElem *kov = mozgo->kov;
        free(mozgo);
        mozgo = kov;
    }
}

// Keresesi Lista memóriaterület felszabadítása
static void keresesi_lista_felszabaditas(KeresesiListaElem *lista)
{
    KeresesiListaElem *mozgo = lista;
    while (mozgo != NULL)
    {
        KeresesiListaElem *kov = mozgo->kov;
        free(mozgo->query);
        free(mozgo);
        mozgo = kov;
    }
}

// Lista kiírása
static void lista_kiiras(ListaElem *lista)
{
    ListaElem *mozgo;
    for (mozgo = lista; mozgo != NULL; mozgo = mozgo->kov)
    {
        printf("%s %s %s %s %s \n", mozgo->adat->vezeteknev, mozgo->adat->keresztnev, mozgo->adat->foglalkozas, mozgo->adat->varos, mozgo->adat->telefonszam);
    }
}

// Megfelelő választás ellenörzés
static bool megfelelo_e(int valaszt, int *lehetosegek, int szam)
{
    bool megfelelo = false;

    if (valaszt == EOF)
        megfelelo = false;

    for (size_t i = 0; i < szam; i++)
    {
        if (lehetosegek[i] == valaszt)
            megfelelo = true;
    }

    return megfelelo;
}

static void csere(ListaElem *a, ListaElem *b)
{
    AdatMezo temp = a->adat[0];
    a->adat[0] = b->adat[0];
    b->adat[0] = temp;
}

// A beolvasott adatokat buborék rendezéssel ABC sorrendbe állítjuk
static void buborek_rendezes(ListaElem *lista)
{
    int index = 0;
    bool kicserelt = false;
    ListaElem *mozgo;
    ListaElem *temp = NULL;

    /* Checking for empty list */
    if (lista == NULL)
        return;

    do
    {
        kicserelt = false;
        mozgo = lista;

        while (mozgo->kov != temp)
        {
            if (strcmp(mozgo->adat->vezeteknev, mozgo->kov->adat->vezeteknev) < 0)
            {
                csere(mozgo, mozgo->kov);
                kicserelt = true;
            }
            mozgo = mozgo->kov;
        }
        temp = mozgo;
    } while (kicserelt);
}

// String beolvasás dinamikus tömbbel
static char *string_beolvasas(bool wildcard)
{
    DinTomb dt;
    dianamikus_lefoglalas(&dt, 1);

    if (wildcard)
        dt.tomb[0] = '*';
    getchar();

    int karakter;
    int i = 0;
    if (wildcard)
        i = 1;
    do
    {
        karakter = getchar();
        if (karakter != '\n')
        {
            i += 1;

            dintomb_atmeretez(&dt, i);
            dt.tomb[i - 1] = karakter;
        }
        else
        {
            if (wildcard)
            {
                dintomb_atmeretez(&dt, i + 2);
                dt.tomb[i] = '*';
                dt.tomb[i + 1] = '\0';
            }
            else
            {
                dintomb_atmeretez(&dt, i + 1);
                dt.tomb[i] = '\0';
            }
        }
    } while (karakter != '\n');

    return dt.tomb;
}

// Kézzel való beolvasáskor, a bevitt mezőket ellenörzi, hogy a formátuma helyes
static bool forma_helyes_bemenet(char *szoveg, int max_char)
{
    if (szoveg != NULL && (strlen(szoveg) - 1) < max_char)
        return true;
    return false;
}

// Kézzel való beolvasás
static ListaElem *beolvasas_kezzel(void)
{
    ListaElem *lista = NULL;
    AdatMezo adatok;

    int kesz = 1;

    while (kesz == 1)
    {
        // Név
        bool helyes_forma = false;
        while (!helyes_forma)
        {
            printf("Név: ");
            scanf("%s %s", adatok.vezeteknev, adatok.keresztnev);

            helyes_forma = forma_helyes_bemenet(adatok.vezeteknev, 255) && forma_helyes_bemenet(adatok.vezeteknev, 255) ? true : false;

            if (!helyes_forma)
                printf("Nem megfelelő a bemeneti formátum!");
        }

        // Foglalkozás
        helyes_forma = false;
        while (!helyes_forma)
        {
            printf("Foglalkozás: ");
            scanf("%s", adatok.foglalkozas);
            helyes_forma = forma_helyes_bemenet(adatok.foglalkozas, 255);

            if (!helyes_forma)
                printf("Nem megfelelő a bemeneti formátum!");
        }

        // Város
        helyes_forma = false;
        while (!helyes_forma)
        {
            printf("Város: ");
            scanf("%s", adatok.varos);
            helyes_forma = forma_helyes_bemenet(adatok.varos, 255);

            if (!helyes_forma)
                printf("Nem megfelelő a bemeneti formátum!");
        }

        // Telefonszám
        helyes_forma = false;
        while (!helyes_forma)
        {
            printf("Telefonszám: ");
            scanf("%s", adatok.telefonszam);
            helyes_forma = forma_helyes_bemenet(adatok.telefonszam, 11);

            if (!helyes_forma)
                printf("Nem megfelelő a bemeneti formátum!");
        }

        // Új adat beolvasása, vagy befejezés
        helyes_forma = false;
        while (!helyes_forma)
        {

            printf("A beolvasás befejezése, vagy újabb adat bevitele? Befejezés [0], Új adat [1] \n");
            scanf("%d", &kesz);
            int lehetosegek[2] = {0, 1};

            helyes_forma = megfelelo_e(kesz, lehetosegek, 2);

            if (!helyes_forma)
                printf("Nem megfelelő a bemeneti formátum! \n");
        }

        // Láncolt lista létrehozása az adatok tárolásához
        ListaElem *uj;
        uj = (ListaElem *)malloc(sizeof(ListaElem));
        if (uj == NULL)
        {
            printf("Nem sikerült memóriát foglalni!\n");
            kilepes(lista);
        }
        uj->kov = lista;
        uj->adat[0] = adatok;
        lista = uj;
    }
    return lista;
}

// Beolvasás fájlból
static ListaElem *beolvasas_fajlbol(char *fajlnev)
{
    FILE *f = fopen(fajlnev, "r");
    if (f == NULL)
    {
        printf("Fájl megnyitása sikertelen, kérlek probáld újra! \n");
        adatok_beolvasasa();
    }
    ListaElem *lista = NULL;
    AdatMezo adatok;

    while (fscanf(f, "%s\t%s\t%s\t%s\t%s\n", adatok.vezeteknev, adatok.keresztnev, adatok.foglalkozas, adatok.varos, adatok.telefonszam) != EOF)
    {
        if (!forma_helyes_bemenet(adatok.vezeteknev, 255) || !forma_helyes_bemenet(adatok.keresztnev, 255) || !forma_helyes_bemenet(adatok.foglalkozas, 255) || !forma_helyes_bemenet(adatok.varos, 255) || !forma_helyes_bemenet(adatok.telefonszam, 11))
        {
            printf("A fájl nem megfelelő formátumú, ellenőrizd kérlek! \n");
            adatok_beolvasasa();
        }

        ListaElem *uj;
        uj = (ListaElem *)malloc(sizeof(ListaElem));
        if (uj == NULL)
        {
            printf("Nem sikerült memóriát foglalni!\n");
            kilepes(lista);
        }
        uj->kov = lista;
        uj->adat[0] = adatok;
        lista = uj;
    }

    buborek_rendezes(lista);

    fclose(f);
    return lista;
}

// Választás párosítása
static char *parositas(int valasztas)
{
    if (valasztas == 1)
        return "Foglalkozás";
    if (valasztas == 2)
        return "Város";
    if (valasztas == 3)
        return "Telefonszám";

    return "Név";
}

// A keresési listából visszaadjuk a tipusnak megfelelő query-t
static char *get_query(KeresesiListaElem *query, int tipus)
{
    KeresesiListaElem *mozgo;
    for (mozgo = query; mozgo != NULL; mozgo = mozgo->kov)
        if (mozgo->index == tipus)
            return mozgo->query;
    return NULL;
}

// Keresés
static ListaElem *kereses_a_listaban(ListaElem *lista, KeresesiListaElem *query, int *tipus, int tipus_db)
{
    ListaElem *szurt_lista = NULL;
    ListaElem *mozgo;

    for (mozgo = lista; mozgo != NULL; mozgo = mozgo->kov)
    {
        int talalat = 0;

        for (size_t i = 0; i < tipus_db; i++)
        {
            // Név alapján keresünk
            if (tipus[i] == 0)
            {
                int veznev_hossz = strlen(mozgo->adat->vezeteknev) - 1;
                int kereszn_hossz = strlen(mozgo->adat->keresztnev) - 1;

                char nev[veznev_hossz + kereszn_hossz + 1];

                strcpy(nev, mozgo->adat->vezeteknev);
                strcat(nev, mozgo->adat->keresztnev);

                talalat = fnmatch(get_query(query, 0), nev, 0);
            }

            // Foglalkozás alapján keresünk
            if (tipus[i] == 1)
                talalat = fnmatch(get_query(query, 1), mozgo->adat->foglalkozas, 0);

            // Város alapján keresünk
            if (tipus[i] == 2)
                talalat = fnmatch(get_query(query, 2), mozgo->adat->varos, 0);

            // Telefonszám alapján keresünk
            if (tipus[i] == 3)
                talalat = fnmatch(get_query(query, 3), mozgo->adat->telefonszam, 0);

            // printf("%d", tipus[i]);
        }

        if (talalat == 0 && talalat != FNM_NOMATCH)
        {
            ListaElem *uj;
            uj = (ListaElem *)malloc(sizeof(ListaElem));
            uj->kov = szurt_lista;
            uj->adat[0] = mozgo->adat[0];
            szurt_lista = uj;

            // printf("'%s' matches '%s'\n", query, mozgo->adat->varos);
        }
    }
    return szurt_lista;
}

// Kilépés a programból
static void kilepes(ListaElem *lista)
{
    lista_felszabaditas(lista);
    printf("Viszont látásra!");

    exit(0);
}

// Beolvasás fajtájának kiválasztása, 0 - Kézzel, 1 - Fájlból
void adatok_beolvasasa(void)
{
    // Beolvasás módjának kiválasztása
    // Az alapvető a kézzel való beolvasás.
    int beolvasas_fajta = 1;
    printf("Hogy szeretné az adatokat beolvasni? [0] Kézzel, [1] Fájlból \n");
    scanf("%d", &beolvasas_fajta);

    int lehetosegek[2] = {0, 1};
    if (!megfelelo_e(beolvasas_fajta, lehetosegek, 2))
    {
        printf("Nem megfelelő a bemeneti formátum! \n");
        printf("Visszatérés a beolvasás elejére... \n");
        adatok_beolvasasa();
    }

    // Lista inicializálása
    ListaElem *lista;

    // Beolvasás kézzel
    if (beolvasas_fajta == 0)
    {
        printf("Kézzel való beolvasás... \n");

        lista = beolvasas_kezzel();

        if (lista == NULL)
        {
            printf("Nem sikerült a beolvasás! \n");
            printf("Visszatérés a beolvasás elejére... \n");
            adatok_beolvasasa();
        }

        else
            fomenu(lista);
    }

    // Beolvasás fájlból
    if (beolvasas_fajta == 1)
    {
        printf("Fájlból való beolvasás... \n");

        bool helyes_file = false;
        char *fajl_nev;
        while (!helyes_file)
        {

            printf("Addja meg a beolvasni kívánt fájl nevét! \n");
            fajl_nev = string_beolvasas(false);
            int szoveg_e = fnmatch("*.txt", fajl_nev, 0);

            if (szoveg_e == 0 && szoveg_e != FNM_NOMATCH)
                helyes_file = true;
            else
                printf("Nem megfelelő fájl formátum, .txt fájlnak kell lennie! \n");
        }
        lista = beolvasas_fajlbol(fajl_nev);

        if (lista == NULL)
        {
            printf("Nem sikerült a beolvasás! \n");
            printf("Visszatérés a beolvasás elejére... \n");
            adatok_beolvasasa();
        }

        else
            fomenu(lista);
    }
}

// Főmenü
void fomenu(ListaElem *lista)
{
    // Az elsődleges választás az egyadatos keresés
    int valasztas = 0;
    printf("Hogy szerete keresést indítani? [0] Egy adatos keresés, [1] Több adatos keresés \n");
    scanf("%d", &valasztas);

    int lehetosegek[2] = {0, 1};
    if (!megfelelo_e(valasztas, lehetosegek, 2))
    {
        printf("Nem megfelelő a bemeneti formátum! \n");
        printf("Visszatérés a főmenü elejére... \n");
        fomenu(lista);
    }

    // Egy adatos keresés
    if (valasztas == 0)
    {
        printf("Egy adatos keresés... \n");
        KeresesiListaElem *egy_adat = NULL;

        int tipus_valasztas = 2;
        printf("Melyik adat alapján szeretne keresni? [0] Név, [1] Foglalkozás, [2] Város, [3] Telefonszám \n");
        scanf("%d", &tipus_valasztas);

        int lehetosegek[4] = {0, 1, 2, 3};
        if (!megfelelo_e(tipus_valasztas, lehetosegek, 4))
        {
            printf("Nem megfelelő a bemeneti formátum! \n");
            printf("Visszatérés a főmenü elejére... \n");
            fomenu(lista);
        }

        char *tipus = parositas(tipus_valasztas);
        printf("Keresés %s alapján ", tipus);

        printf("Mire szeretne rákeresni? \n");
        char *pattern = string_beolvasas(true);

        KeresesiListaElem *uj;
        uj = (KeresesiListaElem *)malloc(sizeof(KeresesiListaElem));
        if (uj == NULL)
        {
            printf("Nem sikerült memóriát foglalni!\n");
            kilepes(lista);
        }
        uj->kov = egy_adat;
        uj->query = pattern;
        uj->index = tipus_valasztas;
        egy_adat = uj;

        int kivalasztott_tipus[1] = {tipus_valasztas};
        ListaElem *szurt_lista = kereses_a_listaban(lista, egy_adat, kivalasztott_tipus, 1);

        // Szürt lista kiírás
        lista_kiiras(szurt_lista);

        lista_felszabaditas(szurt_lista);
        keresesi_lista_felszabaditas(egy_adat);

        menu_3(lista);
    }

    // Több adatok keresés
    if (valasztas == 1)
    {
        printf("Több adatos keresés... \n");

        int tipus_valasztas = 0;
        int tipusok[4] = {0, 1, 2, 3};
        int valasztas_db = 0;
        KeresesiListaElem *tobb_adat = NULL;
        bool inditas = false;
        char *pattern;
        while (!inditas)
        {
            printf("Melyik adat alapján szeretne keresni? ");
            for (size_t i = 0; i < 4; i++)
            {
                if (tipusok[i] != -1)
                {
                    printf("[%d] %s", tipusok[i], parositas(tipusok[i]));
                    if (i != 3)
                        printf(", ");
                }
            }
            printf("\n");

            scanf("%d", &tipus_valasztas);
            if (!megfelelo_e(tipus_valasztas, tipusok, 4))
                printf("Nem megfelelő a bemeneti formátum!");

            for (size_t i = 0; i < 4; i++)
                if (tipusok[i] == tipus_valasztas)
                    tipusok[i] = -1;
            valasztas_db += 1;

            printf("Mire szeretne rákeresni? \n");
            char *pattern = string_beolvasas(true);

            KeresesiListaElem *uj;
            uj = (KeresesiListaElem *)malloc(sizeof(KeresesiListaElem));
            uj->kov = tobb_adat;
            uj->query = pattern;
            uj->index = tipus_valasztas;
            tobb_adat = uj;

            printf("\n");
            int kereses_valasztas = 0;
            if (valasztas_db <= 4)
            {
                printf("Keresés indítása, vagy újabb típus kiválasztása? [0] Keresés indítása, [1] Új tipus kiválasztása \n");

                scanf("%d", &kereses_valasztas);
                int lehetosegek[2] = {0, 1};
                if (!megfelelo_e(kereses_valasztas, lehetosegek, 2))
                    printf("Nem megfelelő a bemeneti formátum!");
            }
            else
            {
                printf("Keresés indítása? [0] Keresés indítása \n");

                scanf("%d", &kereses_valasztas);
                int lehetosegek[1] = {0};
                if (!megfelelo_e(kereses_valasztas, lehetosegek, 1))
                    printf("Nem megfelelő a bemeneti formátum!");
            }

            if (kereses_valasztas == 0)
                inditas = true;
        }

        if (inditas)
        {
            int kivalasztott_tipusok[4] = {-1};
            int j = 0;
            for (size_t i = 0; i < 4; i++)
                if (tipusok[i] == -1)
                    kivalasztott_tipusok[j++] = i;

            ListaElem *szurt_lista = kereses_a_listaban(lista, tobb_adat, kivalasztott_tipusok, valasztas_db);

            // Szürt lista kiírás
            lista_kiiras(szurt_lista);

            lista_felszabaditas(szurt_lista);
            keresesi_lista_felszabaditas(tobb_adat);

            menu_3(lista);
        }
    }
}

// A keresés utáni menü
void menu_3(ListaElem *lista)
{
    int tipus_valasztas = 0;
    printf("Új keresés indítása, Új beolvasás vagy kilépés? [0] Új keresés [1] Új beolvasás [2] Kilépés \n");
    scanf("%d", &tipus_valasztas);

    int lehetosegek[3] = {0, 1, 2};
    if (!megfelelo_e(tipus_valasztas, lehetosegek, 4))
        printf("Nem megfelelő a bemeneti formátum!");

    if (tipus_valasztas == 0)
        fomenu(lista);
    if (tipus_valasztas == 1)
    {
        lista_felszabaditas(lista);
        adatok_beolvasasa();
    }
    if (tipus_valasztas == 2)
        kilepes(lista);
}

// Kiválasztás 1. - Beolvasás módjának kiválasztása
int main(void)
{
    adatok_beolvasasa();

    /*
    char *string = string_beolvasas(false);
    printf("%s", string);
    free(string);
    */

    return 0;
}