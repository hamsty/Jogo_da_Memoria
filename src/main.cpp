#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include <SPIFFS.h>
#include <FS.h>
#include <gamesirg3s.h>
#include <LedTableNxN.h>
#include <vector>
#include <set>
#include <cstdlib>

using namespace std;

#define NUM_LEDS 12
#define MESA 15
#define GAME_VELOCITY 100
#define JOYSTICK_DELAY 300
#define DIF [](int a) { return (a == 2 ? 6 : (a == 1 ? 3 : 2)); }

const char *casa = "Talles";
const char *senha = "talles12345";

static AsyncWebServer server(80);

static JoystickClient *client;

static bool jogoIniciado = false;
static bool dificuldadeEscolhida = false;
static bool jogoTerminado = false;
static int dificuldade = 0;
static int antiga = -1;

static LedTableNxN display(NUM_LEDS, MESA, NEO_GRB + NEO_KHZ800);
static pair<int, int> foco;
static pair<int, int> escolha1;
static pair<int, int> escolhaAnterior;
static set<pair<int, int>> ganhos;
vector<vector<uint16_t>> vetor;

void drawImage(int16_t x, int16_t y, String file)
{
  if (SPIFFS.exists(file))
  {
    fs::File hello_kitty = SPIFFS.open(file);
    uint8_t buffer[2];
    hello_kitty.read(buffer, 2);
    uint16_t height;
    memcpy(&height, buffer, sizeof(uint16_t));
    hello_kitty.read(buffer, 2);
    uint16_t width;
    memcpy(&width, buffer, sizeof(uint16_t));
    uint16_t bitmap[width * height];
    for (int i = 0; i < width * height; i++)
    {
      hello_kitty.read(buffer, 2);
      memcpy(&bitmap[i], buffer, sizeof(uint16_t));
    }
    display.drawRGBBitmap(x, y, bitmap, width, height);
  }
  else
  {
    Serial.println("Arquivo indisponível!");
  }
}

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
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", "text/html", false); });
  server.on("/mesa.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/mesa.js", "text/javascript", false); });
  server.on("/show", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String img;
    for (int i = 0; i < 144; i++)
    {
      char linha[50];
      uint32_t pixel = display.getPixel(i);
      uint8_t red = pixel >> 16, green = pixel >> 8, blue = pixel;
      sprintf(linha, "RGB(%d,%d,%d)|", red, green, blue);
      img.concat(linha);
    }
    request->send_P(200, "text/plain", img.c_str()); });
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/favicon.ico", "image/x-icon", false); });
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

void layerJogo()
{
  int tamanhoP = DIF(dificuldade);
  int tamanhoL = 12 / tamanhoP;
  int n = tamanhoP * tamanhoP / 2;
  uint16_t cores[19] = {0x3AF1, 0xDD57, 0xB4EB, 0x5002, 0x6975, 0xE760, 0x435C, 0xB434, 0x8004, 0xFDE0, 0xCAA0, 0x0539, 0x6C47, 0x0184, 0xC972, 0x3FE2, 0x80EF, 0x03FF, 0x440D};
  vector<int> escolhida;
  vector<int> descolhidas;
  srand((unsigned)time(NULL));
  for (int i = 0; i < n; i++)
  {
    int r = -1;
    do
    {
      r = rand() % 19;
    } while (find(escolhida.begin(), escolhida.end(), r) != escolhida.end());
    escolhida.push_back(r);
  }
  vetor.resize(tamanhoP);
  for (vector<uint16_t> v : vetor)
  {
    for (uint16_t u : v)
    {
      if (!escolhida.empty())
      {
        random_shuffle(escolhida.rbegin(), escolhida.rend());
        u = cores[*escolhida.rbegin()];
        descolhidas.push_back(*escolhida.rbegin());
        escolhida.pop_back();
      }
      else
      {
        random_shuffle(descolhidas.rbegin(), descolhidas.rend());
        u = cores[*descolhidas.rbegin()];
        descolhidas.pop_back();
      }
    }
  }
  for (int i = 0; i < display.width(); i += tamanhoP)
  {
    for (int j = 0; j < display.height(); j += tamanhoP)
    {
      display.fillScreen(0x4208);
    }
  }
}

void layerEscolhas()
{
  display.fillScreen(0X00);
  uint16_t colors[3] = {0x04C0, 0x9CC0, 0x5800};
  colors[dificuldade] += 200;
  for (int i = 0; i < 3; i++)
  {
    display.fillRect(0, i * 4, 12, 4, colors[i]);
  }
  jogoIniciado = true;
}

void layerWin()
{
  display.fillScreen(0X00);
  drawImage(0, 0, "/trofeu.bin");
}

void escolhaQuadrado()
{
  int tamanhoP = DIF(dificuldade);
  int tamanhoL = 12 / tamanhoP;
  pair<int, int> xy = client->getXY();
  int x = xy.first;
  int y = xy.second;
  escolhaAnterior = foco;
  pair<int, int> foco = {(foco.first + x) > tamanhoP - 1 ? 0 : ((foco.first + x < 0) ? tamanhoP - 1 : foco.first + x),
                         (foco.second + y) > tamanhoP - 1 ? 0 : ((foco.second + y < 0) ? tamanhoP - 1 : foco.second + y)};
  Serial.printf("foco(%d %d)\n", foco.first, foco.second);

  if (escolhaAnterior != foco)
  {
    display.fillRect(escolhaAnterior.first * tamanhoL, escolhaAnterior.second * tamanhoL, tamanhoL, tamanhoL, 0x4208);
    display.fillRect(foco.first * tamanhoL, foco.second * tamanhoL, tamanhoL, tamanhoL, 0xFFFF);
  }
  if (client->aPressed())
  {
    if (ganhos.count(foco) == 0)
    {
      // display.fillRect(foco.first*tamanhoL,foco.second*tamanhoL,tamanhoL,tamanhoL, vetor[foco.first][foco.second]);
      if (escolha1 == pair<int, int>(-1, -1))
      {
        escolha1 = foco;
      }
      else
      {
        // if (escolha1 != foco)
        // {
        //   if (vetor[escolha1.first][escolha1.second] == vetor[foco.first][foco.second])
        //   {
        //     ganhos.insert(escolha1);
        //     ganhos.insert(foco);
        //     display.fillRect(foco.first*tamanhoL,foco.second*tamanhoL,tamanhoL,tamanhoL, 0x00);
        //     display.fillRect(escolha1.first*tamanhoL,escolha1.second*tamanhoL,tamanhoL,tamanhoL, 0x00);
        //   }
        //   else
        //   {
        //     display.fillRect(foco.first*tamanhoL,foco.second*tamanhoL,tamanhoL,tamanhoL, 0xFFFF);
        //   }
        // }
        escolha1 = {-1, -1};
      }
    }
  }
  if (ganhos.size() == tamanhoL * tamanhoL)
  {
    jogoTerminado = true;
    layerWin();
  }
  if (client->bPressed())
  {
    if (escolha1 != pair<int, int>(-1, -1))
    {
      escolha1 = {-1, -1};
      display.fillScreen(0x4208);
      display.fillRect(foco.first * tamanhoL, foco.second * tamanhoL, tamanhoL, tamanhoL, vetor[foco.first][foco.second]);
    }
    else
    {
      dificuldadeEscolhida = false;
    }
  }
}

void escolheDificuldade()
{
  antiga = dificuldade;
  pair<int, int> xy = client->getXY();
  int y = xy.first;
  dificuldade = (dificuldade + y) > 2 ? 0 : ((dificuldade + y < 0) ? 2 : dificuldade + y);
  if (antiga != dificuldade)
  {
    layerEscolhas();
  }
  if (client->aPressed())
  {
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
          layerJogo();
          escolhaQuadrado();
        }
        else
        {
          if (client->bPressed() || client->aPressed())
          {
            jogoTerminado = false;
            dificuldadeEscolhida = false;
          }
        }
      }
    }
    // display.show();
    delay(GAME_VELOCITY);
  }
}