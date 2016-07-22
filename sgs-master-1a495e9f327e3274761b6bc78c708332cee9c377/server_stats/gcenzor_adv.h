/***************************************************************************
***************************************************************************
(c) 1999-2006 Ganymede Technologies                 All Rights Reserved
Krakow, Poland                                  www.ganymede.eu
***************************************************************************
***************************************************************************/
#ifndef __gcenzor_adv_h__
#define __gcenzor_adv_h__

#define MAX_CHAT 512
#include <cstddef>
#include <google/dense_hash_map>

using namespace std;
using namespace google;

// zarezerwowana wartosc TOKEN_ZERO, zeby mozna tworzyc stringi ASCIIZ)
#define TOKEN_ZERO			0
#define TOKEN_A				(TOKEN_ZERO + 1)
#define TOKEN_LIMITER		(TOKEN_A + 26)
#define MAX_TOKEN			(TOKEN_LIMITER)

// Wezel automatu, ktory budujemy.
struct state_node
{
	struct state_node *gotos[MAX_TOKEN + 1];
	struct state_node *fail;
	int max_vulgar_len;			// max. dlugosc wyrazu, ktory sie tu konczy
	int node_nr;                // uzywane podczas zamiany na 'ac_gotos_bintables'
};

// Znak o indeksie idx w 'src_canonical_form' odpowiada ciagowi znakow
// canonical_index_map[idx][0]..canonical_index_map[idx][1] w oryginalnym tekscie 'src'.
// Jesli nie odpowiada zadnemu (moze tak byc w przypadku TOKEN_LIMITER) to
// zarowno [0] i [1] sa poza granicami [0..strlen(src_canonical_form) - 1]
typedef int index_map[MAX_CHAT][2];

class GCenzorAdv
{
public: 
	GCenzorAdv();
	bool			Init(string path);

	bool			Test(char *,int);
	bool			Test(string & str);
	bool			ToLower(char *,int);
	bool			TestWide(wchar_t *src);

private:
	void			TokenizeSrc(wchar_t *src, unsigned char *src_canonical_form, index_map& canonical_index_map, int which_map = 0);

private:

	vector<struct state_node*>	state_nodes;
	int							current_state_nodes_count;

	struct state_node			*root;
	int							total_nodes;

	//--- Automat zamieniony na tablice binarnych wyszukiwan ---

	int							*ac_nodes_fail;
	int							*ac_max_vulgar_len;
	// Pod indeksem [0] w tablicy _idx jest jej rozmiar.
	int							*ac_gotos_bintables_idx[MAX_TOKEN + 1];
	int							*ac_gotos_bintables_value[MAX_TOKEN + 1];

	//----------------------------------------------------------

	struct state_node*			get_new_state_node();
	void						add_tokenized_word(unsigned char *word);
	void						build_automata();
	void						ac_search(unsigned char* line, char* censored_mask);
	int							find_goto(int node_nr, unsigned char token_nr);

private:
	int							words_count;

	struct eq_wchar
	{
			bool operator()(wchar_t s1, wchar_t s2) const
			{
				return (s1==s2);
			}
	};

	struct hash_wchar
	{
			size_t operator()(wchar_t x) const { return x;} ;
	};

	// Pierwsza mapa traktuje znaki !$013457@| kolejno jako isoleastai.
	// Druga mapa traktuje powyzsze znaki jako TOKEN_LIMITER.
	dense_hash_map<wchar_t, unsigned char, hash_wchar, eq_wchar > CharactersCanonicalMap[2];

	// Mapa zamiany na male litery
	dense_hash_map<wchar_t, wchar_t, hash_wchar, eq_wchar > ToLowerMap;
};

#endif
