# Abyss Tone – metalowa wtyczka gitarowa

Kompletny łańcuch gitarowy (VST3 / AU / samodzielna aplikacja) zestrojony pod brzmienie w stylu **Sylosis / Josh Middleton**:
zwarty, środkowy rytm high-gain z wyraźnym atakiem palm mute'ów i „ścianą” z dwóch wzmacniaczy.

Działa w każdym DAW obsługującym VST3 (Reaper, Cubase, Ableton Live, FL Studio, Studio One, Bitwig, Ardour…),
w Logic/GarageBand jako AU (macOS) oraz **bez DAW-a** jako samodzielna aplikacja.

## Łańcuch sygnału

```
IN → GATE → OVERDRIVE (TS-808) → WZMACNIACZ (4x oversampling) → KOLUMNA → TIGHT → BUS EQ → DOUBLER → DELAY → REVERB → OUT
                                   └─ opcjonalny drugi wzmacniacz (Layer) ─┘
```

| Moduł | Co robi | Skąd pomysł |
|---|---|---|
| **Gate** | bramka RMS z histerezą, przed wzmacniaczem, zamyka się ~100 ms po stopie | ciche przerwy między riffami |
| **Overdrive** | model TS-808: czysta ścieżka + przesterowana ścieżka środka (~720 Hz) | Josh używa OD808 z drive ≈ 0, tone na zero, level = unity – jako filtra zaciskającego dół przed wzmacniaczem |
| **Wzmacniacz** | 3 kanały: **Clean**, **Thrash** (charakter w stylu 5150 – zwarty, agresywny górny środek), **Modern** (w stylu Rectifiera – luźniejszy dół, więcej nasycenia, „sag”). Gain, Bass, Mid, Treble, Presence, Depth, Master | rytmy nagrywane na 5150 + warstwy na Dual Rectifierze |
| **Layer / Wide L/R** | dokłada drugi głos wzmacniacza; w trybie Wide: lewy kanał = główny wzmacniacz, prawy = drugi | podwójna ściana gitar z dwóch różnych głów |
| **Kolumna** | modele 4x12 V30 (nowoczesna, „vintage 2000s”), otwarta 2x12, **ładowanie własnych IR (.wav)**. Gałka miksu mikrofonów SM57 ↔ MD421 | 57 jako podstawa + odrobina 421 |
| **Tight** | dynamiczne ścinanie pasma 65–294 Hz tylko przy głośnych palm mute'ach | jego kompresja wielopasmowa na szynie gitar |
| **Bus EQ** | 6 pasm: SUB, 300, 700, 2.3K, BITE (3.2 kHz), AIR | sub, wycięcie ~700 Hz i ~2,3 kHz, „bite” na 3 kHz, powietrze |
| **Doubler** | sztuczny dubel (modulowane krótkie opóźnienia) | szerokie rytmy z jednej ścieżki |
| **Delay** | stereo, z synchronizacją do tempa DAW-a | ~360 ms / niski miks do solówek |
| **Reverb** | pogłos stereo | solówki i czyste partie |
| **Tuner** | stroik chromatyczny (wycisza wyjście) | |

### Presety fabryczne
- **Rhythm | Thrash Wall** – główne brzmienie rytmiczne (domyślne)
- **Rhythm | Layered Wide (2 amps)** – Thrash po lewej, Modern po prawej + lekki doubler
- **Rhythm | Modern Chug** – cięższy, luźniejszy dół
- **Lead | Melodic** – solówki: więcej środka, delay 360 ms, pogłos
- **Clean | Atmospheric** – czyste, przestrzenne intro
- **Doom | Slow Crush** – wolne, ciężkie riffy

Własne presety: przyciski **SAVE / LOAD** (pliki `.abyss` w `Dokumenty/Abyss Tone/Presets`).

### Wskazówki do brzmienia Sylosis
1. Overdrive: Drive 0, Tone 0, Level 0 dB – nie dodaje przesteru, tylko zaciska dół i pcha środek.
2. Wzmacniacz Thrash: Gain 6–7, Mid ~4 (kolumna i mikrofon i tak są środkowe), Treble ~4,5 (unikasz ostrości 1–2 kHz), Presence wysoko (7–9).
3. Mic blend: 10–20% MD421 – więcej daje kłujące 4,3 kHz.
4. Nagraj **dwie osobne ścieżki** rytmu i rozłóż L/P – to daje największą szerokość. Layer/Doubler to skrót, gdy masz jedną ścieżkę.
5. Strój: do niskich strojów zostaw Tight włączony – trzyma dół w ryzach.

## Kompilacja

Wymagania: **CMake 3.22+** i kompilator C++17. JUCE pobiera się automatycznie (lub wrzuć go do folderu `JUCE/`).

**Windows** (Visual Studio 2022 z komponentem „Desktop development with C++”):
```bat
cmake -S . -B build
cmake --build build --config Release --target AbyssTone_All
```

**macOS** (Xcode + Command Line Tools):
```bash
cmake -S . -B build -G Xcode
cmake --build build --config Release --target AbyssTone_All
```

**Linux**:
```bash
sudo apt install libasound2-dev libjack-jackd2-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target AbyssTone_All -j
```

**Bez kompilowania lokalnie:** wrzuć ten folder do repozytorium na GitHubie – workflow w `.github/workflows/build.yml`
sam zbuduje wersje na Windows, macOS i Linux (zakładka *Actions* → *Artifacts*).

Wyniki: `build/AbyssTone_artefacts/Release/` → `VST3/`, `AU/` (macOS), `Standalone/`.

## Instalacja

| System | VST3 | AU |
|---|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` | – |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/Components/` |
| Linux | `~/.vst3/` | – |

Potem przeskanuj wtyczki w DAW-ie. Wtyczka nie jest podpisana – na macOS po pierwszym skopiowaniu:
`xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Abyss Tone.vst3"` (analogicznie dla `.component` i aplikacji).

### Bez DAW-a (Standalone)
Uruchom aplikację **Abyss Tone**, kliknij **Options → Audio/MIDI Settings**, wybierz interfejs audio,
wejście (gitara) i wyjście, ustaw jak najmniejszy bufor (64–128 próbek). Na Windows wybierz tryb **Windows Audio (Exclusive Mode)**
dla niskiej latencji.

## Testy
`cmake -S . -B build -DABYSS_BUILD_TESTS=ON` buduje `AbyssRenderTest` – renderuje syntetyczną gitarę przez wszystkie presety
do plików WAV i wypisuje poziomy, widmo i obciążenie CPU. Wtyczka przechodzi **pluginval** na poziomie rygoru 10.

## Licencja
Kod wtyczki jest Twój. Wtyczka korzysta z frameworka **JUCE 8** (AGPLv3 lub licencja komercyjna) – do użytku własnego bez ograniczeń;
przy sprzedaży/dystrybucji zamkniętego kodu potrzebna jest licencja JUCE.

Nazwy 5150, Rectifier, TS-808, SM57, MD421, V30 i Sylosis służą wyłącznie do opisu inspiracji brzmieniowej; projekt nie jest
powiązany z ich właścicielami ani z STL Tones.
