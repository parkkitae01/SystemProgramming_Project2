# SIC Assembler (Project 2)

이 프로젝트는 **시스템 프로그래밍 과제용 간단한 SIC 어셈블러**입니다.  
교수님이 제공하신 `inst.data.txt` 명령어 테이블을 이용하여 PASS1 / PASS2 과정을 수행합니다.  
SIC(XE 아님) 기본 명령만 처리하며, 프로그램은 `C` 언어로 작성되었습니다.

---

## 📁 파일 구성
| 파일 | 설명 |
|------|------|
| `my_assembler.c` | 어셈블러 소스 코드 |
| `inst.data.txt` | 명령어 테이블 (교수님 제공 파일) |
| `source.asm` | 입력 소스 프로그램 (교재 예제 Fig 2.1 기반) |
| `output.lst` | 실행 결과 리스트 파일 (교재 Fig 2.2 형식) |

---

## ⚙️ 실행 방법
1️⃣ GCC로 컴파일:
```bash
gcc -std=c11 -O2 my_assembler.c -o myasm.exe
```

2️⃣ 프로그램 실행 (리스트 파일 생성):
```bash
./myasm.exe inst.data.txt source.asm > output.lst
```

3️⃣ 생성된 `output.lst`를 열면  
   주소, 오브젝트 코드, 원본 라인이 교재 예제와 같은 형식으로 출력됩니다.

---

## 🧠 프로그램 개요
- **PASS1:**  
  - START 구문으로 시작 주소 설정  
  - 각 라벨(Label)의 주소를 계산하여 심볼 테이블(SYMTAB)에 저장  

- **PASS2:**  
  - inst.data에서 opcode를 읽어 명령어를 오브젝트 코드로 변환  
  - 주소와 함께 리스트 파일(`output.lst`)로 출력  

---

## 📖 테스트 예제
교재 예제인 **Fig 2.1 (COPY 프로그램)** 을 `source.asm`으로 입력했을 때,  
결과(`output.lst`)는 **Fig 2.2와 동일한 형식**으로 출력됨을 확인하였습니다.

---

## 📌 참고
- 과제명: *Project #2 — Control Section-based Assembler (SIC)*  
- 과목명: *시스템 프로그래밍 (System Programming)*  
- 작성 언어: *C (GCC 15.2, MSYS2 환경)*  
- 확장 목표: Project #3에서 XE 확장(Format 4, EXTDEF/EXTREF) 추가 예정
