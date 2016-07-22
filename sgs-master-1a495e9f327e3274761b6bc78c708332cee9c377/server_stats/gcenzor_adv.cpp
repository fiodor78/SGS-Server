/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/

#include "headers_all.h"
#include "utils_conversion.h"
#include "gcenzor_adv.h"

// Po ile wezlow naraz alokowac w pamieci, zeby bylo szybciej
#define STATE_NODES_CHUNK		50000

// algorytm Aho-Corasicka
struct state_node* GCenzorAdv::get_new_state_node()
{
    struct state_node* s;

	if (state_nodes.size() == 0 || current_state_nodes_count == STATE_NODES_CHUNK)
	{
		state_nodes.resize(state_nodes.size() + 1);
		state_nodes[state_nodes.size() - 1] = new state_node[STATE_NODES_CHUNK];
		memset(state_nodes[state_nodes.size() - 1], 0, sizeof(state_node) * STATE_NODES_CHUNK);
		current_state_nodes_count = 0;
	}

	s = &state_nodes[state_nodes.size() - 1][current_state_nodes_count++];
	total_nodes++;
	return s;
}

void GCenzorAdv::add_tokenized_word(unsigned char *word)
{
	struct state_node *q;
	unsigned char *p;
	int i;

	//--- tworzymy automat i wypelniamy pola 'gotos' ----------------------

	p = (unsigned char*)word;
	q = root;

	while (*p != 0)
	{
		if (q->gotos[*p] == NULL)
			q->gotos[*p] = get_new_state_node();
		q = q->gotos[*p];
		p++;
	}

	q->max_vulgar_len = max(q->max_vulgar_len, (int)(p - (unsigned char*)word));

	for (i = 0; i <= MAX_TOKEN; i++)
		if (root->gotos[i] == NULL)
			root->gotos[i] = root;
}

void GCenzorAdv::build_automata()
{
    int i, j, goto_counts[MAX_TOKEN + 1];
    struct state_node *q, *parent, *r;

    struct state_node **kolejka;
    int kolejka_start, kolejka_end;

//--- wypelniamy pola 'fail' -----------------------------------------

    typedef struct state_node* p_state_node;
    kolejka = new p_state_node[total_nodes + 2];

    kolejka[0] = root;
    root->node_nr = 0;
    kolejka_start = kolejka_end = 1;

    root->fail = NULL;
    for(i = 0; i <= MAX_TOKEN; i++)
        if (root->gotos[i] != root)
        {
            root->gotos[i]->fail = root;
            root->gotos[i]->node_nr = kolejka_end;
            kolejka[kolejka_end++] = root->gotos[i];
        }

    while (kolejka_start != kolejka_end)
    {
        parent = kolejka[kolejka_start++];

        for(i = 0; i <= MAX_TOKEN; i++)
            if (parent->gotos[i] != NULL)
            {
                q = parent->gotos[i];
                q->node_nr = kolejka_end;
                kolejka[kolejka_end++] = q;

                r = parent->fail;
                while (r != NULL && r->gotos[i] == NULL)
                    r = r->fail;

                if (r == NULL)
                    q->fail = root->gotos[i];
                else
                {
                    q->fail = r->gotos[i];
					q->max_vulgar_len = max(q->max_vulgar_len, q->fail->max_vulgar_len);
                }
            }
    }

//--- Konwertujemy automat do postaci 'ac_gotos_hashmap' ----------------------
//--- Uwaga w hashmapie wszystkie wezly sa numerowane od 1, root = 1(!) ---

	ac_nodes_fail = new int[total_nodes + 2];
	ac_max_vulgar_len = new int[total_nodes + 2];

	for (i = 0; i < total_nodes; i++)
	{
		if (kolejka[i]->fail == NULL)
			ac_nodes_fail[i + 1] = 0;
		else
			ac_nodes_fail[i + 1] = kolejka[i]->fail->node_nr + 1;

		ac_max_vulgar_len[i + 1] = kolejka[i]->max_vulgar_len;
	}

	memset(goto_counts, 0, sizeof(goto_counts));
    for (i = 0; i < total_nodes; i++)
    {
        for (j = 0; j <= MAX_TOKEN; j++)
            if (kolejka[i]->gotos[j] != NULL)
                goto_counts[j]++;
	}

	for (j = 0; j <= MAX_TOKEN; j++)
	{
		ac_gotos_bintables_idx[j] = new int[goto_counts[j] + 1];
		ac_gotos_bintables_value[j] = new int[goto_counts[j] + 1];
		ac_gotos_bintables_idx[j][0] = 0;
	}

    for (i = 0; i < total_nodes; i++)
    {
        for (j = 0; j <= MAX_TOKEN; j++)
            if (kolejka[i]->gotos[j] != NULL)
			{
				ac_gotos_bintables_idx[j][++ac_gotos_bintables_idx[j][0]] = i + 1;
				ac_gotos_bintables_value[j][ac_gotos_bintables_idx[j][0]] = kolejka[i]->gotos[j]->node_nr + 1;
			}
	}

    delete kolejka;
}

int GCenzorAdv::find_goto(int node_nr, unsigned char token_nr)
{
	int l, r, k;

	l = 0;
	r = ac_gotos_bintables_idx[token_nr][0] + 1;

	do
	{
		k = (l + r) / 2;
		if (ac_gotos_bintables_idx[token_nr][k] <= node_nr)
			l = k;
		else
			r = k;
	} while (r != l + 1);

	if (l < 0 || l >= ac_gotos_bintables_idx[token_nr][0] + 1 || ac_gotos_bintables_idx[token_nr][l] != node_nr)
		return 0;

	return ac_gotos_bintables_value[token_nr][l];
}

// Funkcja ac_search() szuka w tekscie 'line' wulgaryzmow.
// W 'censored_line' umieszcza linijke skladajaca sie z dwoch znakow '.' i '*'.
// Pierwszy oznacza tekst ok, drugi tekst do ocenzurowania.

void GCenzorAdv::ac_search(unsigned char* line, char* censored_mask)
{
    unsigned char *p, *pend;
	int q_nr;

	p = (unsigned char*)censored_mask;
	for (pend = line; *pend != 0; pend++)
		*p++ = '.';
	*p = 0;
	if (*(p = line) == 0)
		return;

	q_nr = 1;
	q_nr = find_goto(q_nr, TOKEN_LIMITER);	// wiemy, ze z roota na pewno sa wszystkie gotos

    while (p < pend && *p == TOKEN_LIMITER)
        p++;

    for (; p < pend; p++)
    {
		while (q_nr != 0 && find_goto(q_nr, *p) == 0)
			q_nr = ac_nodes_fail[q_nr];
		if (q_nr == 0)
			q_nr = 1;
		else
		{
			q_nr = find_goto(q_nr, *p);
			if (ac_max_vulgar_len[q_nr] > 0)
			{
				// TOKEN_LIMITERow z poczatku i konca nie zamieniamy na '*'
				for (int i = 1; i < ac_max_vulgar_len[q_nr] - 1; i++)
					censored_mask[(p - line) - i] = '*';
            }
        }
    }
}

/***************************************************************************/

GCenzorAdv::GCenzorAdv()
{
	ac_nodes_fail = NULL;
	ac_max_vulgar_len = NULL;
	for (int i = 0; i <= MAX_TOKEN; i++)
	{
		ac_gotos_bintables_idx[i] = NULL;
		ac_gotos_bintables_value[i] = NULL;
	}

	words_count=0;
};

bool GCenzorAdv::ToLower(char * src_utf,int max_len)
{
	int caps_words_count;		// liczba slow napisanych z przewaga duzych liter
	int total_words_count;		// liczba wszystkich slow
	int current_word_caps, current_word_length;
	wchar_t	src[MAX_CHAT], *src_ptr;

	wcsncpy(src, A2Ws(src_utf)(), MAX_CHAT);
	src[MAX_CHAT - 1] = 0;

	total_words_count = caps_words_count = 0;

	src_ptr = src; 
	while (*src_ptr != 0)
	{
		while (*src_ptr != 0 && (CharactersCanonicalMap[0][*src_ptr] == 0 || CharactersCanonicalMap[0][*src_ptr] == TOKEN_LIMITER))
			src_ptr++;
		if (*src_ptr == 0)
			break;

		current_word_caps = current_word_length = 0;
		while (*src_ptr != 0 && (CharactersCanonicalMap[0][*src_ptr] != 0 && CharactersCanonicalMap[0][*src_ptr] != TOKEN_LIMITER))
		{
			if (ToLowerMap[*src_ptr] != 0)
				current_word_caps++;
			current_word_length++;
			src_ptr++;
		}

		if (current_word_length <= 1 && current_word_caps == 0)
			continue;

		if (current_word_caps >= current_word_length / 2)
			caps_words_count++;
		total_words_count++;
	}

	if ((total_words_count == 2 && caps_words_count == 2) ||
		(total_words_count > 2 && caps_words_count >= (total_words_count + 1) / 2))
	{
		for (src_ptr = src; *src_ptr != 0; src_ptr++)
			if (ToLowerMap[*src_ptr] != 0)
				*src_ptr = ToLowerMap[*src_ptr];
	}

	strncpy(src_utf, W2Us(src)(), max_len);
	src_utf[max_len - 1] = 0;

	return false;
}

bool GCenzorAdv::Test(char * src_utf,int max_len)
{
	wchar_t	src[MAX_CHAT];

	if(words_count==0) return false;

	wcsncpy(src, A2Ws(src_utf)(), MAX_CHAT);
	src[MAX_CHAT - 1] = 0;

	bool changed=TestWide(src);

	strncpy(src_utf, W2Us(src)(), max_len);
	src_utf[max_len - 1] = 0;

	return changed;
}

bool GCenzorAdv::Test(string & str_utf)
{
	wchar_t	src[MAX_CHAT];
	char dest_utf[MAX_CHAT];

	if(words_count==0) return false;

	wcsncpy(src, A2Ws(str_utf.c_str())(), MAX_CHAT);
	src[MAX_CHAT - 1] = 0;

	bool changed=TestWide(src);

	strncpy(dest_utf, W2Us(src)(), MAX_CHAT);
	dest_utf[MAX_CHAT - 1] = 0;

	str_utf=dest_utf;
	return changed;
}

// Funkcja TokenizeSrc pobiera ciag znakow znajdujacy sie pod 'src'
// i w 'src_canonical_form' umieszcza jego forme kanoniczna (TOKEN_*)
// Na poczatku i koncu ciagu dodaje TOKEN_LIMITER.
// Ponadto wypelnia tez tablice canonical_index_map[] odpowiednimi danymi.
// Parametr 'which_map' okresla, ktorej z map zamiany na tokeny mamy uzyc.

void GCenzorAdv::TokenizeSrc(wchar_t *src, unsigned char *src_canonical_form, index_map& canonical_index_map, int which_map)
{
	wchar_t * src_ptr;
	unsigned char *p, *pforward;
	int idx;

	src_ptr = src;
	p = src_canonical_form;
	idx = 0;
	// Sprowadzamy caly ciag znakow do ciagu malych liter + znaki TOKEN_LIMITER.
	*p++ = TOKEN_LIMITER;
	canonical_index_map[idx][0] = -1;
	canonical_index_map[idx++][1] = -1;
	while (*src_ptr)
	{
		if (CharactersCanonicalMap[which_map][*src_ptr] == 0)
			*p++ = TOKEN_LIMITER;
		else
			*p++ = CharactersCanonicalMap[which_map][*src_ptr];

		canonical_index_map[idx][0] = (int)(src_ptr - src);
		canonical_index_map[idx++][1] = (int)(src_ptr - src);
		src_ptr++;
	}
	*p++ = TOKEN_LIMITER;
	canonical_index_map[idx][0] = (int)(src_ptr - src);
	canonical_index_map[idx++][1] = (int)(src_ptr - src);
	*p = 0;

	// Usuwamy proste powtorzenia liter/TOKEN_LIMITERow
	p = pforward = src_canonical_form;
	idx = 0;
	while (*pforward != 0)
	{
		canonical_index_map[p - src_canonical_form][0] = canonical_index_map[pforward - src_canonical_form][0];
		while (*p == *pforward)
			pforward++;
		if (*pforward != 0)
			canonical_index_map[p - src_canonical_form][1] = canonical_index_map[pforward - src_canonical_form][0] - 1;
		else
			canonical_index_map[p - src_canonical_form][1] = canonical_index_map[pforward - 1 - src_canonical_form][1];
		*++p = *pforward;
	}
	*++p = 0;

	// Wyodrebniamy fragmenty typu x-x (gdzie x to pojedyncza litera)
	// i zamieniamy je na pojedyncze wystapienie -x-.
	// Przyklady:
	//   -dupa-i-ijas-		==> -dupa-i-jas-
	//   -kur-rwa-			==> -ku-r-wa-
	//   -kur-r-r-wa-			==> -ku-r-wa-
	//   -ok-u-u-u-u-r-w-w-w-wa- ==> -ok-u-r-w-a-

	p = pforward = src_canonical_form;
	while (*pforward != 0)
	{
		if ((pforward[0] != TOKEN_LIMITER && pforward[0] != 0) && pforward[1] == TOKEN_LIMITER && pforward[2] == pforward[0])
		{
			char c = pforward[0];
			int firstidx = canonical_index_map[pforward - src_canonical_form][0];

			if (p[-1] != TOKEN_LIMITER)
			{
				canonical_index_map[p - src_canonical_form][0] = -1;
				canonical_index_map[p - src_canonical_form][1] = -1;
				*p++ = TOKEN_LIMITER;
			}

			canonical_index_map[p - src_canonical_form][0] = firstidx;

			pforward += 2;
			while (*pforward == c && pforward[1] == TOKEN_LIMITER)
			{
				canonical_index_map[p - src_canonical_form][1] = canonical_index_map[pforward - src_canonical_form][1];
				pforward += 2;
			}

			if (*pforward == c)
			{
				canonical_index_map[p - src_canonical_form][1] = canonical_index_map[pforward - src_canonical_form][1];
				pforward++;
			}

			*p++ = c;

			canonical_index_map[p - src_canonical_form][0] = canonical_index_map[pforward - 1 - src_canonical_form][0];
			canonical_index_map[p - src_canonical_form][1] = canonical_index_map[pforward - src_canonical_form][0] - 1;
			*p++ = TOKEN_LIMITER;	

			continue;
		}

		canonical_index_map[p - src_canonical_form][0] = canonical_index_map[pforward - src_canonical_form][0];
		canonical_index_map[p - src_canonical_form][1] = canonical_index_map[pforward - src_canonical_form][1];
		*p++ = *pforward++;
	}
	*p = 0;

	//------- Wypisujemy debug info -----

//		char ds[10000];
//		*ds = 0;
//		for (int i = 0; i < p - src_canonical_form; i++)
//			sprintf(ds + strlen(ds), "IDX %d: '%c' --> {%d, %d}\r\n", i, src_canonical_form[i], canonical_index_map[i][0], canonical_index_map[i][1]);
}

bool GCenzorAdv::TestWide(wchar_t *src)
{
	wchar_t src_censored[MAX_CHAT], src_tmp[MAX_CHAT];
	unsigned char src_canonical_form[MAX_CHAT];
	char censored_mask[MAX_CHAT], *p;
	bool changed = false;
	int src_len, i;
	index_map canonical_index_map;

	wcscpy(src_censored, src);
	src_len = (int)wcslen(src);

	// Kazda linijke sprawdzamy w 4 przebiegach:
	//   a) mapa 0 - zamieniamy znaki podobne do liter na litery
	//	 b) mapa 1 - traktujemy znaki rozne od liter jako TOKEN_LIMITER
	//	 c) jak a) tylko wspak
	//	 d) jak b) tylko wspak
	// [Uwaga 14/12/2006: wylaczamy przeszukiwanie wspak.]
	for (int pass = 0; pass <= 1 /* 4 */; pass++)
	{
		if (pass == 2 || pass == 4)
		{
			for (i = 0; i < src_len; i++)
				src_tmp[i] = src[src_len - 1 - i];
			src_tmp[src_len] = 0;
			wcscpy(src, src_tmp);

			for (i = 0; i < src_len; i++)
				src_tmp[i] = src_censored[src_len - 1 - i];
			src_tmp[src_len] = 0;
			wcscpy(src_censored, src_tmp);

			if (pass == 4)
				break;
		}

		TokenizeSrc(src, src_canonical_form, canonical_index_map, pass % 2);

		// Szukamy wulgaryzmow
		ac_search(src_canonical_form, censored_mask);

		// Jesli cos zostalo zagwiazdkowane to gwiazdkujemy to na oryginalnym slowie i zwracamy
		for (p = censored_mask; *p != 0; p++)
		{
			if (*p == '*')
			{
				for (i = canonical_index_map[p - censored_mask][0]; i <= canonical_index_map[p - censored_mask][1]; i++)
					if (i >= 0 && i < src_len)
					{
						src_censored[i] = '*';
						changed = true;
					}
			}
		}
	}

	if (changed)
		wcscpy(src, src_censored);

	return changed;
}

bool GCenzorAdv::Init(string path_)
{
	int i, c, j;
	static int hashmaps_initialized = 0;

	if (hashmaps_initialized == 0)
	{
		hashmaps_initialized = 1;

		// Budujemy mapy zamiany znakow na tokeny.

		// Napierw duze litery z ogonkami, potem odpowiadajace im male(!)
		// Wazne, bo tablica ta jest rowniez wykorzystywana do zamiany duzych liter na male.
		int znaki_ogonki[] = { 
								0x0104, 'a', 0x0105, 'a',
								0x0106, 'c', 0x0107, 'c',
								0x0118, 'e', 0x0119, 'e',
								0x0141, 'l', 0x0142, 'l',
								0x0143, 'n', 0x0144, 'n',
								0x00d3, 'o', 0x00f3, 'o',
								0x015a, 's', 0x015b, 's',
								0x0179, 'z', 0x017a, 'z',
								0x017b, 'z', 0x017c, 'z',
		};

		int znaki_specjalne[] = {
									'!', 'i', '$', 's', '0', 'o', '1', 'l', '@', 'a', '|', 'i',
									/* '3', 'e', '4', 'a', '5', 's', '7', 't', */
		};

		for (i = 0; i < 2; i++)
		{
			CharactersCanonicalMap[i].set_empty_key((wchar_t)0);

			for (c = 'a'; c <= 'z'; c++)
			{
				CharactersCanonicalMap[i][(wchar_t)c] = (unsigned char)(TOKEN_A + c - 'a');
				CharactersCanonicalMap[i][(wchar_t)(c - 'a' + 'A')] = (unsigned char)(TOKEN_A + c - 'a');
			}

			for (j = 0; j < sizeof(znaki_ogonki) / sizeof(int); j += 2)
				CharactersCanonicalMap[i][(wchar_t)znaki_ogonki[j]] = (unsigned char)(TOKEN_A + znaki_ogonki[j + 1] - 'a');
		}

		//	Pierwsza mapa bierze pod uwage znaki specjalne mogace udawac litery
		for (j = 0; j < sizeof(znaki_specjalne) / sizeof(int); j += 2)
			CharactersCanonicalMap[0][(wchar_t)znaki_specjalne[j]] = (unsigned char)(TOKEN_A  + znaki_specjalne[j + 1] - 'a');

		//--- Wypelniamy hash zamiany znakow na male litery
		ToLowerMap.set_empty_key((wchar_t)0);
		for (c = 'a'; c <= 'z'; c++)
			ToLowerMap[(wchar_t)(c - 'a' + 'A')] = (wchar_t)c;

		for (j = 0; j < sizeof(znaki_ogonki) / sizeof(int); j += 4)
			ToLowerMap[(wchar_t)znaki_ogonki[j]] = (wchar_t)znaki_ogonki[j + 2];
	}

	//--- Usuwamy z pamieci ew. istniejacy automat (np. w przypadku reloadu pliku ze slowami) ---
	if (ac_nodes_fail != NULL)
		delete ac_nodes_fail;
	if (ac_max_vulgar_len != NULL)
		delete ac_max_vulgar_len;
	ac_nodes_fail = NULL;
	ac_max_vulgar_len = NULL;

	for (i = 0; i <= MAX_TOKEN; i++)
	{
		if (ac_gotos_bintables_idx[i] != NULL)
			delete ac_gotos_bintables_idx[i];
		if (ac_gotos_bintables_value[i] != NULL)
			delete ac_gotos_bintables_value[i];
		ac_gotos_bintables_idx[i] = NULL;
		ac_gotos_bintables_value[i] = NULL;
	}

	//--- Wczytujemy slowa z pliku ---
	total_nodes = 0;
	state_nodes.clear();

	// (!) Do testow!
//	static int max_instances_count = 0;
//	if (++max_instances_count > 1)
//		return false;

	wchar_t src[MAX_CHAT];
	unsigned char src_canonical_form[MAX_CHAT];
	index_map canonical_index_map;
	FILE * f;

	f = fopen(path_.c_str(), "rb");
	if (f == NULL)
	{
		root = NULL;
		return false;
	}

	root = get_new_state_node();

	char line[128];
	while(fgets(line,128,f))
	{
		char * end10=strchr(line,10);if(end10) *end10=0;
		char * end13=strchr(line,13);if(end13) *end13=0;
		if (line[0])
		{
			wcsncpy(src, A2Ws(line)(), MAX_CHAT);
			src[MAX_CHAT - 1] = 0;

			TokenizeSrc(src, src_canonical_form, canonical_index_map);

			// Teraz dodajemy do automatu wszystkie mozliwe odmiany slowa w 'src'.
			// 'Odmiany' roznia sie wystepowaniem TOKEN_LIMITER'ow.
			unsigned char *src_ptr, *dst_ptr;
			unsigned char new_word_form[MAX_CHAT];

			int len;

			len = (int)strlen((char*)src_canonical_form);
			if (len < 3)
				continue;

			// (!) Do testow - zeby sie szybciej uruchamialo.
			if (len > 80)
				continue;

			// Bedziemy wstawiac TOKEN_LIMITER
			// Ale zeby nie tworzyc za duzo roznych odmian danego wyrazu (bo braknie
			// nam pamieci) to ograniczamy sie do:
			// a) wszystkie odmiany skladajace sie z max. 2 TOKEN_LIMITERow
			// b) odmiana z TOKEN_LIMITERem po kazdym znaku i max. 2 mniej
			// Czyli wyrazy moga byc podzielone na max. 3 kawalki (a),
			// lub wszystkie litery osobno, za wyjatkiem moze 2 "zbitkow" (b)
			for (i = 0; i < 1 << (len - 3); i++)
			{
				int jedynki = 0;
				for (j = 0; j < len - 3; j++)
					if ((i & (1 << j)) > 0)
						jedynki++;
				if (jedynki > 2 && jedynki < len - 3 - 2)
					continue;

				src_ptr = src_canonical_form;
				dst_ptr = new_word_form;

				for (j = 0; j < len; j++)
				{
					*dst_ptr++ = *src_ptr++;
					// Wstawiamy TOKEN_LIMITER po znaku
					if (j > 0 && (i & (1 << (j - 1))) > 0)
					{
						if (src_canonical_form[j] == TOKEN_LIMITER)
							break;
						*dst_ptr++ = TOKEN_LIMITER;
					}
				}
				*dst_ptr = 0;

				if (j < len)
					continue;

				add_tokenized_word(new_word_form);
				words_count++;
			}
		}
	}
	fclose(f);

	build_automata();

	for (i = 0; i < (int)state_nodes.size(); i++)
		delete state_nodes[i];
	state_nodes.clear();

	return (words_count != 0);
}
