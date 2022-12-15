#include <Arduino.h>
#include <engineLed12x12.h>
#include <WiFi.h>
#include <pgmspace.h>
#include <WiFiClient.h>
#include <FastLED.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <gamesirg3s.h>
#include <vector>
#include <set>

using namespace std;

#define NUM_LEDS 144
#define MESA 16
#define GAME_VELOCITY 100
#define JOYSTICK_DELAY 300
#define FOCUS_COLOR CRGB::Grey
#define HARD_COLOR CRGB::Red
#define MEDIUM_COLOR CRGB::Blue
#define EASY_COLOR CRGB::Green
#define GAME_COLORS                                                                                                                                                                                                                                                                                                                                  \
  {                                                                                                                                                                                                                                                                                                                                                  \
    CRGB::DarkOrange, CRGB::DarkCyan, CRGB::DarkGoldenrod, CRGB::DarkKhaki, CRGB::DarkMagenta, CRGB::DarkOliveGreen, CRGB::DarkViolet, CRGB::DarkTurquoise, CRGB::DarkSalmon, CRGB::DarkRed, CRGB::DarkCyan, CRGB::MediumAquamarine, CRGB::Azure, CRGB::Lavender, CRGB::LavenderBlush, CRGB::LightBlue, CRGB::LightCoral, CRGB::Purple, CRGB::Sienna \
  }
#define DIF [](int a) { return (a == 2 ? 2 : (a == 1 ? 3 : 6)); }

const uint32_t winimg[12][12] PROGMEM = {
    {4278190080, 4278190080, 4278190080, 4288773275, 4288643231, 4289958300, 4289892763, 4288643231, 4288773275, 4278190080, 4278190080, 4278190080},
    {4278190080, 4278190080, 4289353130, 4290353051, 4293773975, 4294234775, 4294234775, 4293708184, 4290287515, 4289353130, 4278190080, 4278190080},
    {4278190080, 4294902015, 4290089884, 4293839768, 4292129948, 4291867556, 4293711285, 4292459167, 4293839767, 4290024091, 4294902015, 4278190080},
    {4278190080, 4286545834, 4291405465, 4292524699, 4293316527, 4294897615, 4294698937, 4294765250, 4292721819, 4291405467, 4286545834, 4278190080},
    {4278190080, 4289431964, 4293971351, 4292459426, 4294831297, 4294698937, 4294897359, 4294766032, 4291999401, 4293971351, 4289431964, 4278190080},
    {4278190080, 4288708507, 4292129432, 4292986021, 4294633145, 4294897099, 4294698938, 4294962890, 4292722853, 4292063897, 4288969880, 4278190080},
    {4278190080, 4294902015, 4291208090, 4294365847, 4292921265, 4293777590, 4293843906, 4293052850, 4294366103, 4291208090, 4294902015, 4278190080},
    {4278190080, 4278190080, 4289037468, 4291537050, 4294037143, 4293642907, 4293577115, 4294102935, 4291602841, 4289037468, 4278190080, 4278190080},
    {4278190080, 4278190080, 4278190080, 4289623705, 4291263127, 4291597465, 4291597465, 4291197591, 4289688987, 4278190080, 4278190080, 4278190080},
    {4278190080, 4278190080, 4278190080, 4289949338, 4293357972, 4292635795, 4292635795, 4293358229, 4289949338, 4278190080, 4278190080, 4278190080},
    {4278190080, 4278190080, 4278190080, 4289818523, 4293752985, 4293031834, 4293031834, 4293752985, 4289818523, 4278190080, 4278190080, 4278190080},
    {4278190080, 4278190080, 4278190080, 4289490334, 4290081180, 4288703898, 4288703898, 4289950109, 4289490334, 4278190080, 4278190080, 4278190080}};

const char *casa = "Talles";
const char *senha = "talles12345";

static AsyncWebServer server(80);

static CRGB mesa[NUM_LEDS] PROGMEM;

static JoystickClient *client;

static bool jogoIniciado = false;
static bool dificuldadeEscolhida = false;
static bool jogoTerminado = false;
static int dificuldade = 0;
static int antiga = -1;

static Engine *jogo;
static pair<int, int> foco;
static pair<int, int> escolha1;
static pair<int, int> escolhaAnterior;
static set<pair<int, int>> ganhos;

static uint32_t newmesa[144];

void setup()
{
  Serial.begin(115600);
  WiFi.begin(casa, senha);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  if (!SPIFFS.begin(true))
  {
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  fs::File root = SPIFFS.open("/", 0);
  while (fs::File file = root.openNextFile())
  {
    Serial.println(file.name());
  }
  FastLED.addLeds<NEOPIXEL, MESA>(mesa, NUM_LEDS);
  for (int i = 0; i < 144; i++)
  {
    mesa[i] = CRGB::White;
  }
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html", false);
  });
  server.on("/mesa.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/mesa.js", "text/javascript", false);
  });
  server.on("/zelda.jpg", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/zelda.jpg", "image/jpeg", false);
  });
  server.on("/show", HTTP_GET, [](AsyncWebServerRequest *request) {
    String img PROGMEM;
    for (int i = 0; i < 144; i++)
    {
      char linha[50] PROGMEM;
      sprintf(linha, "RGB(%d,%d,%d)|", mesa[i].r, mesa[i].g, mesa[i].b);
      img.concat(linha);
    }
    request->send_P(200, "text/plain", img.c_str());
  });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/favicon.ico", "image/x-icon", false);
  });
  server.begin();
  Serial.println("Servidor Iniciado");
  Serial.print("Acessa em casa http://");
  Serial.println(WiFi.localIP());
  client = new JoystickClient();
  while (!client->isFind())
  {
  }
  Serial.println("Joystick Achado!");
}

Layer layerJogo()
{
  int tamanhoP = DIF(dificuldade);
  int tamanhoL = 12 / tamanhoP;
  Layer layer;
  Patterns patterns = (*layer.setPatterns(tamanhoL, tamanhoL));
  int n = tamanhoP * tamanhoP / 2;
  uint32_t cores[19] = GAME_COLORS;
  set<int> escolhida;
  for (int i = 0; i < n; i++)
  {
    int r = -1;
    do
    {
      r = random8(0, 18);
    } while (escolhida.count(r) > 0);
    escolhida.insert(r);
  }
  vector<Pattern> vetor(tamanhoL * tamanhoL);
  random_shuffle(vetor.begin(), vetor.end());
  auto it = vetor.begin();
  auto sit = escolhida.begin();
  int c = 0;
  for (; it != vetor.end(); it++)
  {
    Pattern quadrado;
    quadrado.setPattern(tamanhoP, tamanhoP, TypePattern::pattern, dificuldade == 0 ? HARD_COLOR : (dificuldade == 1 ? MEDIUM_COLOR : EASY_COLOR));
    quadrado.setPattern(tamanhoP, tamanhoP, TypePattern::focusPattern, FOCUS_COLOR);
    quadrado.setPattern(tamanhoP, tamanhoP, TypePattern::selectPattern, cores[(*sit)]);
    quadrado.setPattern(tamanhoP, tamanhoP, TypePattern::selectedFocusPattern, cores[(*sit)] - FOCUS_COLOR);
    c++;
    if (c == 2)
    {
      sit++;
    }
    (*it) = quadrado;
  }
  for (int i = 0; i < layer.size().first; i++)
  {
    for (int j = 0; j < layer.size().second; j++)
    {
      patterns[i][j] = (*vetor.rbegin());
      vetor.pop_back();
    }
  }
  return layer;
}

Layer layerEscolhas()
{
  Pattern hard;
  SubPattern pattern = (*hard.setPattern(12, 4, TypePattern::pattern));
  SubPattern focusPattern = (*hard.setPattern(12, 4, TypePattern::focusPattern));
  Layer layer;
  Patterns patterns = (*layer.setPatterns(3, 1));
  for (int i = 0; i < 12; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      if (div(i, 12).rem == 2 || div(i, 12).rem == 3 || div(i, 12).rem == 5 || div(i, 12).rem == 6 || div(i, 12).rem == 8 || div(i, 12).rem == 9)
      {
        pattern[i][j] = HARD_COLOR;
        focusPattern[i][j] = HARD_COLOR;
      }
      else
      {
        focusPattern[i][j] = FOCUS_COLOR;
      }
    }
  }
  patterns[0][0] = hard;
  Pattern medium;
  pattern = (*medium.setPattern(12, 4, TypePattern::pattern));
  focusPattern = (*medium.setPattern(12, 4, TypePattern::focusPattern));
  for (int i = 0; i < 12; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      if (div(i, 12).rem == 3 || div(i, 12).rem == 4 || div(i, 12).rem == 7 || div(i, 12).rem == 8)
      {
        pattern[i][j] = MEDIUM_COLOR;
        focusPattern[i][j] = MEDIUM_COLOR;
      }
      else
      {
        focusPattern[i][j] = FOCUS_COLOR;
      }
    }
  }
  patterns[1][0] = medium;
  Pattern easy;
  pattern = (*easy.setPattern(12, 4, TypePattern::pattern));
  focusPattern = (*easy.setPattern(12, 4, TypePattern::focusPattern));
  for (int i = 0; i < 12; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      if (div(i, 12).rem == 5 || div(i, 12).rem == 6)
      {
        pattern[i][j] = EASY_COLOR;
        focusPattern[i][j] = EASY_COLOR;
      }
      else
      {
        focusPattern[i][j] = FOCUS_COLOR;
      }
    }
  }
  patterns[2][0] = easy;
  jogoIniciado = true;
  return layer;
}

Layer layerWin()
{
  Pattern win;
  SubPattern pattern = (*win.setPattern(12, 12));
  Layer layer;
  Patterns patterns = (*layer.setPatterns(1, 1));
  for (int j = 0; j < 12; j++)
  {
    for (int i = 0; i < 12; i++)
    {
      pattern[i][j] = winimg[i][j];
    }
  }
  patterns[0][0] = win;
  return layer;
}

void escolhaQuadrado()
{
  Layer layer = jogo->top();
  int tamanhoL = layer.size().first;
  pair<int, int> xy = client->getXY();
  int x = xy.first;
  int y = xy.second;
  Serial.printf("escolheQuadrado(%d, %d)\n",xy.first, xy.second);
  escolhaAnterior = foco;
  pair<int, int> foco = {(foco.first + x) > tamanhoL ? 0 : ((foco.first + x < 0) ? tamanhoL : foco.first + x),
                         (foco.second + y) > tamanhoL ? 0 : ((foco.second + y < 0) ? tamanhoL : foco.second + y)};
  Patterns patterns = (*layer.getPatterns());
  patterns[foco.first][foco.second].handleFocus();
  patterns[foco.first][foco.second].handleFocus();
  if (client->aPressed())
  {
    if (ganhos.count(foco) == 0)
    {
      patterns[foco.first][foco.second].handleSelect();
      if (escolha1 == pair<int, int>(-1, -1))
      {
        escolha1 = foco;
      }
      else
      {
        if (escolha1 != foco)
        {
          if (patterns[escolha1.first][escolha1.second].getPattern()[0] == patterns[foco.first][foco.second].getPattern()[0])
          {
            ganhos.insert(escolha1);
            ganhos.insert(foco);
          }
          else
          {
            patterns[foco.first][foco.second].handleSelect();
            patterns[escolha1.first][escolha1.second].handleSelect();
          }
        }
        escolha1 = {-1, -1};
      }
    }
  }
  if (ganhos.size() == tamanhoL * tamanhoL)
  {
    jogoTerminado = true;
    jogo->fowardLayer(layerWin());
  }
  if (client->bPressed())
  {
    if (escolha1 != pair<int, int>(-1, -1))
    {
      escolha1 = {-1, -1};
      patterns[escolha1.first][escolha1.second].handleSelect();
    }
    else
    {
      jogo->backLayer();
      dificuldadeEscolhida = false;
    }
  }
}

void escolheDificuldade()
{
  Layer layer = jogo->top();
  antiga = dificuldade;
  pair<int, int> xy = client->getXY();
  Serial.printf("escolheDificuldade(%d, %d)\n",xy.first, xy.second);
  int y = xy.second;
  dificuldade = (dificuldade + y) > 2 ? 0 : ((dificuldade + y < 0) ? 2 : dificuldade + y);
  Patterns patterns = (*layer.getPatterns());
  patterns[dificuldade][0].handleFocus();
  patterns[antiga][0].handleFocus();
  if (client->aPressed())
  {
    jogo->fowardLayer(layerJogo());
    dificuldadeEscolhida = true;
  }
}

void loop()
{

  if (!client->isConnected())
  {
    client->connectToServer();
  }
  else
  {
    if (!jogoIniciado)
    {
      jogo = Engine::start(layerEscolhas());
      jogoIniciado = true;
      Serial.println("Jogo Iniciado!");
    }
    else
    {
      client->update();
      if (!dificuldadeEscolhida)
      {
        escolheDificuldade();
      }
      else
      {
        if (!jogoTerminado)
        {
          escolhaQuadrado();
        }
        else
        {
          if (client->bPressed() || client->aPressed())
          {
            jogo->backLayer();
            jogoTerminado = false;
            dificuldadeEscolhida = false;
          }
        }
      }
    }
    jogo->print(newmesa);
    for(int i =0;i <144;i++){
      mesa[i] = newmesa[i];
    }
    FastLED.show();
    delay(GAME_VELOCITY);
  }
}