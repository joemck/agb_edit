/* AGB_edit: Edit GBA VC cias. Supports both NSUI injects and Nintendo Ambassador VCs.
 * Currently supports displaying info, adding auto sleep buttons, and editing ghosting.
 *
 * TODO:
 *  - more features :)
 *
 * by Chupi on GBAtemp.net
 */

#include "gbacia.h"
#include "console_ui.h"

//just a table for remembering the status for each file, to print a report at the end
struct entry {
	const char *name;
	const char *status;
};

int main(int argc, char **argv) {
	int nFiles = argc-1;
	if(nFiles < 1) {
		printf(
"Drag one or more GBA VC .cia files to this program's icon or pass them on the\n"
"command line and I'll ask what you want to do with them. This program can show\n"
"info, dump cia contents, dump GBA ROM, or make these sorts of changes:\n\n"
" - Set a button combo for AGB_FIRM to automatically press when you close the\n"
"    lid. Set this to the same as what you set for a sleep patch to get a\n"
"    proper working GBA sleep mode.\n\n"
" - Change video ghosting effect.\n\n"
" - Change video darken effect.\n\n"
);
		system("pause");
		return 1;
	}

	struct entry *files = alloca(nFiles * sizeof(struct entry));
	if(!files) { perror("Can't allocate memory!"); system("pause"); return 1; }

	printf("%d input file%s given.\n\n", nFiles, nFiles==1?" was":"s were");
	if(!doQuestionnaire()) {
		system("pause");
		return 0;
	}

	for(int i=0; i<nFiles; i++) {
		files[i].name = argv[i+1];
		files[i].status = process(argv[i+1]);
		cleanup();
	}

	printf("\n\n\n ==== FINISHED! STATUS REPORT ====\n");
	for(int i=0; i<nFiles; i++)
		printf("%40s => %s\n", files[i].name, files[i].status);
	printf(" ==== DONE ====\n");

	system("pause");
	return 0;
}
