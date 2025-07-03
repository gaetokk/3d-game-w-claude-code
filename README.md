# Claude Code만 이용한 3D 게임

![게임 플레이 영상](./gameplay.gif)

사내 AI툴 해커톤으로 제작한 간단한 3D 게임입니다.
[raylib](https://www.raylib.com/examples.html)에서 다운받은 캐릭터, 나무 3D 리소스만 가져와서 클로드 코드로만 게임을 제작했습니다.
캐릭터가 움직이면서 상단의 수학 문제에 대한 답이 적힌 나무를 베어 점수를 얻는 게임입니다.
저는 코드를 한 줄도 작성하지 않았습니다.

## Features

- **3D 캐릭터 움직임**: WASD 또는 방향키로 캐릭터 이동
- **애니메이션 캐릭터**: 골격 애니메이션을 지원하는 GLB 모델
- **3인칭 카메라**: 캐릭터를 따라가는 부드러운 카메라
- **애니메이션 시스템**: 대기와 걷기 애니메이션 자동 전환
- **수동 애니메이션 제어**: T/G 키로 애니메이션 수동 전환

## Building

### Prerequisites
- raylib library installed
- GCC compiler

### macOS
```bash
# Install raylib via Homebrew
brew install raylib

# Build and run
make run

# Or build and run manually
make
./character_game
```

### Linux
```bash
# Install raylib (Ubuntu/Debian)
sudo apt install libraylib-dev

# Build and run
make run

# Or build and run manually
make
./character_game
```

## Controls

- **WASD** 또는 **방향키**: 캐릭터 이동
- **T**: 다음 애니메이션 (수동 모드)
- **G**: 이전 애니메이션 (수동 모드)

## Assets

전체 애니메이션 캐릭터를 사용하려면:
1. [raylib 예제](https://github.com/raysan5/raylib/tree/master/examples/models/resources/models/gltf)에서 `greenman.glb`, `greenman_*.glb` 모델을 다운로드하세요
2. `assets/models/`에 파일을 배치하세요

모델이 없으면 게임은 간단한 빨간색 큐브를 캐릭터로 사용합니다.

## Project Structure

```
game-test/
├── src/
│   └── main.c          # Main game code
├── assets/
│   ├── models/         # 3D models (GLB format)
│   └── shaders/        # Custom shaders
├── Makefile           # Build configuration
└── README.md          # This file
```

## Known Issues

- 문제에 대한 정답이 없는 상황 있음
- 음수 정답에 대한 처리 불가

## Price

- sonnet api 기준으로 디버깅(삽질)까지 포함해서 약 8.5달러 사용하였습니다.