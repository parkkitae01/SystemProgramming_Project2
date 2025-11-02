// my_assembler.c  -  간단 SIC 어셈블러
// 빌드:  gcc -std=c11 -O2 my_assembler.c -o myasm
// 실행:  ./myasm inst.data.txt input.asm > output.lst
//
// 메모:
//  - 교수님이 준 형식(token_unit, inst_unit 등)을 그대로 씀.
//  - XE 확장 말고 기본 SIC만 처리(명령은 3바이트).
//  - PASS1: 주소 계산하고 라벨을 표에 저장.
//  - PASS2: inst.data에서 opcode 읽어서 기계어로 바꾸고 출력.
//  - 출력은 주소, 오브젝트 코드, 원본 라인 순서로 보이게 함.

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINES   5000
#define MAX_INST    256
#define MAX_OPERAND 3
#define MAX_TOKLEN  128

// 원본 소스 라인을 저장하는 배열
char *input_data[MAX_LINES];
static int line_num;

// 한 줄을 나눠 담는 구조(라벨, 연산자, 피연산자, 주석)
struct token_unit {
    char *label;
    char *operator;
    char  operand[MAX_OPERAND][20];
    char  comment[100];
};
typedef struct token_unit token;
token *token_table[MAX_LINES];

// 명령어 정보(inst.data에서 읽어옴)
struct inst_unit {
    char str[10];      // 예) LDA, STA, RSUB
    unsigned char op;  // opcode (상위 1바이트)
    int format;        // 여기서는 3으로 생각
    int ops;           // 피연산자 개수(대충 구분용)
};
typedef struct inst_unit inst;
inst *inst_table[MAX_INST];
int inst_index = 0;

// 심볼 테이블: 라벨과 주소 저장
typedef struct { char name[32]; int addr; } symrec;
symrec symtab[MAX_LINES];
int symcnt = 0;

int find_sym(const char* name) {
    for (int i=0;i<symcnt;i++) if (!strcmp(symtab[i].name, name)) return i;
    return -1;
}
void add_sym(const char* name, int addr) {
    if (find_sym(name) >= 0) return; // 중복 방지
    strncpy(symtab[symcnt].name, name, sizeof(symtab[symcnt].name)-1);
    symtab[symcnt].name[sizeof(symtab[symcnt].name)-1] = 0;
    symtab[symcnt].addr = addr;
    symcnt++;
}

// 자잘한 유틸 함수들
static inline int isblankline(const char* s){
    while (*s) { if (!isspace((unsigned char)*s)) return 0; s++; }
    return 1;
}
static inline void rstrip(char* s){
    int n = (int)strlen(s);
    while (n>0 && (s[n-1]=='\r'||s[n-1]=='\n')) s[--n]=0;
}
static inline int tohex3(int v, char* out){ // 3바이트를 6자리 16진수로
    return sprintf(out, "%02X%02X%02X", (v>>16)&0xFF, (v>>8)&0xFF, v&0xFF);
}
static inline int tohex2(int v, char* out){
    return sprintf(out, "%04X", v & 0xFFFF);
}
static inline int tohex1(int v, char* out){
    return sprintf(out, "%02X", v & 0xFF);
}
static inline int htoi(const char* s){ int x=0; sscanf(s, "%x", &x); return x; }
static inline int stoi(const char* s){ return atoi(s); }

// inst.data.txt 읽어서 명령어 표 만들기
// 예: "LDA M 3 00"
int load_inst_table(const char* path){
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr,"[ERR] cannot open inst.data: %s\n", path); return -1; }
    char line[256];
    while (fgets(line, sizeof(line), f)){
        if (isblankline(line) || line[0]=='#') continue;
        char mnem[32], kind[8], form[8], ophex[32];
        if (sscanf(line,"%31s %7s %7s %31s", mnem, kind, form, ophex) != 4) continue;

        inst *I = (inst*)calloc(1,sizeof(inst));
        strncpy(I->str, mnem, sizeof(I->str)-1);
        I->str[sizeof(I->str)-1] = 0;
        I->op = (unsigned char)htoi(ophex);
        I->format = (form[0]>='0'&&form[0]<='9') ? atoi(form) : 3;
        I->ops = 1;                  // 기본값
        if (!strcmp(mnem,"RSUB")) I->ops = 0;

        inst_table[inst_index++] = I;
        if (inst_index >= MAX_INST) break;
    }
    fclose(f);
    return 0;
}
inst* find_inst(const char* mnem){
    for (int i=0;i<inst_index;i++){
        if (!strcmp(inst_table[i]->str, mnem)) return inst_table[i];
    }
    return NULL;
}

// 소스 파일을 줄 단위로 읽어서 input_data에 저장
int load_source(const char* path){
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr,"[ERR] cannot open source: %s\n", path); return -1; }
    char buf[1024];
    line_num = 0;
    while (fgets(buf, sizeof(buf), f) && line_num < MAX_LINES){
        input_data[line_num] = (char*)malloc(strlen(buf)+1);
        strcpy(input_data[line_num], buf);
        line_num++;
    }
    fclose(f);
    return 0;
}

// 한 줄을 라벨, 연산자, 피연산자로 나누기
// 첫 토큰이 명령어이면 라벨 없음, 아니면 첫 토큰을 라벨로 본다.
void tokenize_line(int idx){
    char *src = input_data[idx];
    if (!src) return;
    token *t = (token*)calloc(1, sizeof(token));
    token_table[idx] = t;

    // 주석 라인(".")은 건너뜀
    char line[1024]; strncpy(line, src, sizeof(line)-1);
    if (line[0]=='.'){ t->comment[0]='.'; t->comment[1]=0; return; }

    // 토큰 뽑기
    char tok1[MAX_TOKLEN]={0}, tok2[MAX_TOKLEN]={0}, rest[512]={0};
    sscanf(line, " %127s %511[^\n]", tok1, rest);
    if (rest[0]){
        sscanf(rest, " %127s %511[^\n]", tok2, rest);
    }

    // 첫 토큰이 명령어인지 확인
    inst *I1 = find_inst(tok1);
    if (I1){
        // 라벨 없는 줄
        t->label = NULL;
        t->operator = strdup(tok1);

        // 나머지 부분에서 피연산자 채움
        if (tok2[0]) {
            char operands[512]={0};
            if (rest[0]) snprintf(operands,sizeof(operands), "%s %s", tok2, rest);
            else snprintf(operands,sizeof(operands), "%s", tok2);

            int k = 0;
            char *q = strtok(operands, ", \t\r\n");
            while (q && k < MAX_OPERAND){
                strncpy(t->operand[k], q, 19);
                t->operand[k][19] = 0;
                k++;
                q = strtok(NULL, ", \t\r\n");
            }
        }
    } else {
        // 라벨 있는 줄
        if (tok1[0]) t->label = strdup(tok1);   // 환경에 따라 _strdup가 아니라 strdup 사용
        if (tok2[0]) t->operator = strdup(tok2);
        if (rest[0]){
            int k=0; 
            char *q = strtok(rest, ", \t\r\n");
            while(q && k<MAX_OPERAND){
                strncpy(t->operand[k], q, 19);
                t->operand[k][19] = 0;
                k++;
                q = strtok(NULL, ", \t\r\n");
            }
        }
    }
}

// PASS1: 시작 주소 찾고, 줄을 돌면서 라벨 주소를 기록하고 LOC를 증가
typedef struct { int loc_start; int prog_len; int start_addr; char prog_name[32]; } header_info;
header_info H;

// 지시어 크기 계산(BYTE/WORD/RESB/RESW)
int size_of_directive(const char* op, const char* opr0){
    if (!strcmp(op,"WORD")) return 3;
    if (!strcmp(op,"RESW")) return 3 * stoi(opr0);
    if (!strcmp(op,"RESB")) return stoi(opr0);
    if (!strcmp(op,"BYTE")){
        // C'EOF' 는 문자 수만큼, X'F1' 은 2자리가 1바이트
        if (opr0 && opr0[0]=='C' && opr0[1]=='\''){
            const char* s=strchr(opr0,'\''); const char* e=strrchr(opr0,'\'');
            return (s&&e&&e>s)? (int)(e-s-1) : 1;
        } else if (opr0 && opr0[0]=='X' && opr0[1]=='\''){
            const char* s=strchr(opr0,'\''); const char* e=strrchr(opr0,'\'');
            int n = (s&&e&&e>s)? (int)(e-s-1) : 2;
            return (n+1)/2;
        }
        return 1;
    }
    return 0;
}

void pass1(){
    int loc = 0;

    // START 줄 먼저 찾기
    for (int i=0;i<line_num;i++){
        if (!input_data[i]) continue;
        tokenize_line(i);
        token *t = token_table[i];
        if (!t || !t->operator) continue;
        if (!strcmp(t->operator,"START")){
            H.start_addr = htoi(t->operand[0]);
            H.loc_start  = H.start_addr;
            if (token_table[i]->label) {
                strncpy(H.prog_name, token_table[i]->label, 31);
                H.prog_name[31] = 0;
                add_sym(H.prog_name, H.start_addr); // 프로그램 이름도 심볼로 넣음
            }
            loc = H.start_addr;
            break;
        }
    }

    // 본문 훑으면서 주소 배정
    for (int i=0;i<line_num;i++){
        token *t = token_table[i];
        if (!t || !t->operator) continue;
        if (!strcmp(t->operator,"START")) continue;
        if (!strcmp(t->operator,"END"))   break;

        if (t->label && t->label[0] && find_sym(t->label)<0){
            add_sym(t->label, loc);
        }

        inst *I = find_inst(t->operator);
        if (I){
            loc += 3; // SIC 명령은 3바이트
        }else{
            loc += size_of_directive(t->operator, t->operand[0]);
        }
    }
    H.prog_len = loc - H.loc_start;
}

// PASS2: opcode와 주소를 합쳐서 3바이트 코드 만들기
// 형식: [8비트 opcode][1비트 x][15비트 address]
int encode_SIC(const char* op, const char* opr0, int loc, int *out_code){
    (void)loc; // 현재 단순 구현에선 loc 미사용
    inst *I = find_inst(op);
    if (!I){ *out_code = -1; return 0; }

    int opcode = I->op;
    int xbit = 0;
    int addr = 0;

    // 첫 번째 피연산자만 사용(간단 구현)
    char opr[64]={0};
    if (opr0 && opr0[0]) strncpy(opr, opr0, 63);

    // RSUB 같은 케이스
    if (opr[0]==0){
        *out_code = (opcode<<16);
        return 3;
    }

    // 라벨이면 표에서 주소, 아니면 숫자로 처리
    int idx = find_sym(opr);
    if (idx >= 0) {
        addr = symtab[idx].addr;
    } else {
        if (isalpha((unsigned char)opr[0])){
            addr = 0; // 못 찾으면 0(여기선 간단히 처리)
        } else {
            addr = atoi(opr);
        }
    }

    int code = (opcode << 16) | (xbit << 15) | (addr & 0x7FFF);
    *out_code = code;
    return 3;
}

// 두 번째 피연산자가 X이면 인덱스 모드
int has_index_X(token* t){
    return (t && !strcmp(t->operand[1], "X"));
}

// 리스트 파일로 뽑기: 주소, 오브젝트 코드, 원본 라인
void pass2_and_list(){
    int loc = H.loc_start;

    for (int i=0;i<line_num;i++){
        token *t = token_table[i];
        if (!t || (t->comment[0]=='.' && t->operator==NULL)) continue;

        if (!t->operator){
            continue;
        }

        if (!strcmp(t->operator,"START")){
            printf("%04X          %s", loc, input_data[i]);
            continue;
        }
        if (!strcmp(t->operator,"END")){
            printf("              %s", input_data[i]);
            break;
        }

        inst *I = find_inst(t->operator);
        char obj[32]={0};

        if (I){
            int addr = 0;
            if (t->operand[0][0]){
                int sidx = find_sym(t->operand[0]);
                if (sidx>=0) addr = symtab[sidx].addr;
                else if (isalpha((unsigned char)t->operand[0][0])) addr = 0;
                else addr = atoi(t->operand[0]);
            }
            int xbit = has_index_X(t) ? 1 : 0;
            int code3 = (I->op << 16) | (xbit<<15) | (addr & 0x7FFF);

            tohex3(code3, obj);
            printf("%04X  %-6s  %s", loc, obj, input_data[i]);
            loc += 3;
        } else {
            if (!strcmp(t->operator,"WORD")){
                int v = stoi(t->operand[0]);
                tohex3(v & 0xFFFFFF, obj);
                printf("%04X  %-6s  %s", loc, obj, input_data[i]);
                loc += 3;
            } else if (!strcmp(t->operator,"BYTE")){
                // C'..' 또는 X'..' 그대로 바이트로 찍기
                if (t->operand[0][0]=='C' && t->operand[0][1]=='\''){
                    const char* s=strchr(t->operand[0],'\''), *e=strrchr(t->operand[0],'\'' );
                    int n=(int)(e-s-1);
                    printf("%04X  ", loc);
                    for (int k=0;k<n;k++){
                        printf("%02X", (unsigned char)s[1+k]);
                    }
                    printf("    %s", input_data[i]);
                    loc += n;
                } else if (t->operand[0][0]=='X' && t->operand[0][1]=='\''){
                    const char* s=strchr(t->operand[0],'\''), *e=strrchr(t->operand[0],'\'' );
                    char hexbuf[64]={0};
                    int n=(int)(e-s-1);
                    for (int k=0;k<n && k<(int)sizeof(hexbuf)-1;k++) hexbuf[k]=s[1+k];
                    int bytes=(n+1)/2;
                    printf("%04X  ", loc);
                    for (int b=0;b<bytes;b++){
                        int val=0; sscanf(hexbuf+2*b,"%02X",&val);
                        printf("%02X", val);
                    }
                    printf("    %s", input_data[i]);
                    loc += bytes;
                } else {
                    printf("%04X            %s", loc, input_data[i]);
                }
            } else if (!strcmp(t->operator,"RESW")){
                printf("                %s", input_data[i]);
                loc += 3 * stoi(t->operand[0]);
            } else if (!strcmp(t->operator,"RESB")){
                printf("                %s", input_data[i]);
                loc += stoi(t->operand[0]);
            } else {
                // 여기서는 다른 지시어는 그냥 지나감
                printf("%04X            %s", loc, input_data[i]);
            }
        }
    }
}

// 메인: inst.data 읽고, 소스 읽고, PASS1 -> PASS2
int main(int argc, char** argv){
    if (argc < 3){
        fprintf(stderr, "Usage: %s <inst.data.txt> <input.asm>\n", argv[0]);
        fprintf(stderr, "Note : outputs listing to stdout (redirect to file).\n");
        return 1;
    }
    if (load_inst_table(argv[1])<0) return 2;
    if (load_source(argv[2])<0) return 3;

    for (int i=0;i<line_num;i++) tokenize_line(i);
    pass1();
    pass2_and_list();

    // 간단 과제라 메모리 해제는 생략
    return 0;
}
