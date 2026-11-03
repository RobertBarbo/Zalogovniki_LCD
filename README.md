# Centrometal Zalogovniki LCD

Firmware **1.1.4** za ESP32 in ILI9488, avtor prvotnega projekta: Barbo.

Dva zalogovnika s po tremi temperaturnimi senzorji DS18B20, lokalna spletna stran, MQTT Discovery in ArduinoOTA.

## Spremembe v 1.1.4

- Odobren spletni videz iz `preview/web-lcd-v3`: izvirna slika LCD v5, brez oznak položajev in T1–T6 ter odstranjenih dodatnih napisov. Gumb za menjavo slike se imenuje **Slika LCD**.
- Temperature na spletu in barvna legenda uporabljajo iste odtenke: 30 °C modra, 60 °C vijolična, 90 °C rdeča. Med točkami se RGB interpolira; zunaj 30–90 °C ostane skrajna barva. Neveljavne meritve ostanejo `—`. Čas delovanja je zapisan brez vejic.
- Spletni **Ponovni zagon** zahteva potrditev in pošlje en POST ukaz. Stran potrdi uspeh šele, ko API vrne drugačen `boot_id`; izgubljen odgovor ne povzroči samodejnega ponavljanja ukaza.
- Nova MQTT Discovery entiteta **Ponovni zagon**, povezana z obstoječo napravo. Dodana sta ukazni topic in razpoložljivost; obstoječi senzorji, topic-i in identifikatorji so ohranjeni.
- Skupni modul za restart počaka vsaj 750 ms za HTTP odgovor in ne prekine aktivnega prenosa slike ali OTA. Podvojena zahteva ne podaljšuje čakanja.
- LCD izris, ROM preslikava, slika v5, konfiguracija plošče, knjižnice in poverilnice niso spremenjeni. Nova spletna stran je vključena v aplikacijo; upload filesystema ni potreben.

## Spremembe v 1.1.3

- Popravljena fizična položaja po uporabnikovem ponovnem pregledu: `buffer4` / ROM `…3C:A4` je **desni sredina**, `buffer5` / ROM `…3C:62` pa **desni zgoraj**.
- LCD in glavni spletni pregled izpišeta buffer5 v zgornjem desnem polju in buffer4 v srednjem desnem polju. Podstran senzorjev in kopirani seznam uporabljata popravljeni oznaki.
- ROM dodelitve, branje senzorjev, vrstni red podatkov v API-jih, MQTT topici in Discovery identifikatorji ostajajo isti kot v 1.1.2. Spremeni se položaj izpisa, ne identiteta bufferja.
- Ozadje v5 in videz spletnega vmesnika sta ohranjena. Prenova spletnega vmesnika in gumb za ponovni zagon nista del tega popravka.

## Spremembe v 1.1.2

- Stalna preslikava šestih ROM naslovov po uporabnikovem fizično popravljenem seznamu.
- Takratna oznaka desnega zgornjega in srednjega položaja je bila napačna; popravek položajev je opisan pri 1.1.3. ROM dodelitvi buffer4 in buffer5 sta ohranjeni.
- Temperature se berejo neposredno po ROM naslovih iz `include/sensor_mapping.h`. Sprememba vrstnega reda na vodilu ali manjkajoč senzor ne premakne drugih merilnih mest.
- Podstran senzorjev ohrani dodeljeni ROM tudi ob napaki meritve. Indeks je samo podatek iz začetnega pregleda ob zagonu, nikoli vir dodelitve.
- Odstranjeni so stari naslovi z obratnim vrstnim redom bajtov in neučinkoviti klici nastavljanja resolucije na te naslove. Nova koda ohrani nastavitve ločljivosti na fizičnih senzorjih; ne zapisuje nove resolucije.
- Preverjeni so prepis vseh šestih ROM naslovov, njihova unikatnost in CRC. LCD v5, MQTT topici in Discovery identifikatorji ter poverilnice ostajajo ohranjeni.

## Spremembe v 1.1.1

- Nova bralna podstran `/senzorji`, dostopna prek gumba **Senzorji** na glavnem pregledu.
- Za vseh šest trenutnih bufferjev prikaže dejanski 64-bitni ROM naslov Dallas/OneWire, temperaturo, indeks in sedanjo programsko oznako položaja.
- Naslov je zajet pri istem klicu, ki prebere pripadajočo temperaturo. Funkcija izvede enake korake kot obstoječi `getTempCByIndex` (`getAddress` in `getTempC`); indeksi in vrstni red branja ostajajo enaki.
- Endpoint `/api/sensors` vrača samo zadnji vzorec; spletna stran ne sproža dodatnih meritev ali ponovnega iskanja naprav.
- Število 1-Wire naprav je vrednost, zaznana ob zagonu. Stran prikaže šest trenutno spremljanih mest; morebitne druge naprave niso samodejno dodeljene.
- Gumb **Kopiraj seznam** pripravi naslove in trenutne dodelitve za identifikacijo fizičnih položajev. Običajni HTTP ima podprto tudi ročno kopiranje.
- To še ni urejevalnik preslikave. Dodelitev ni mogoče spreminjati prek te strani; nova stalna preslikava po ROM naslovih sledi po določitvi fizičnih položajev.
- Med identifikacijo senzorjev ne odklapljaj, saj sedanje branje po indeksih ob spremembi naprav na vodilu lahko spremeni vrstni red.
- LCD v5, MQTT topici/Discovery, povezave, poverilnice, particijska nastavitev in OTA ostajajo ohranjeni. Uporabnik je potrdil delovanje predhodne različice po svojem flashanju; 1.1.1 še ni naložena s tem postopkom.

## Spremembe v 1.1.0

- Potrjena responsive spletna stran z dejanskimi temperaturami, Wi-Fi podatki, uptime, MQTT stanjem in verzijo firmware-a.
- Osveževanje podatkov vsakih 5 sekund. Ob izgubi povezave se zadnji podatki označijo kot zastareli; neveljavne meritve so prikazane kot `—`.
- Potrjeno ozadje LCD **v5**, brez napisov in IP-ja, s šestimi temperaturami in dinamično Wi-Fi ikono.
- Spletna stran, JavaScript in v5 so vključeni v aplikacijski firmware. Za to različico **ni potreben upload filesystema**.
- V nastavitvah ostane menjava slike LCD. Sprejet je baseline JPG, točno 480 × 320, največ 128 KB. Celotna slika se dekodira pred aktivacijo, prejšnja custom slika pa se shrani za obnovo ob prekinjenem zapisu.
- Runtime ponovno povezovanje Wi-Fi ne prekrije temperatur in ne vsebuje dolgih zank z `delay`. MQTT poskusi so omejeni na en poskus vsakih 5 sekund, s kratkimi timeouti. Discovery se ponovi po povezavi.
- SPIFFS se ob neuspelem mountu **ne formatira**; privzeti LCD, spletna stran in OTA ostanejo na voljo iz aplikacije.
- Verzija ima en vir: `include/firmware_version.h`; uporabljajo jo splash screen, API in MQTT Discovery.

## Struktura

| Datoteka | Namen |
|---|---|
| `src/main.cpp` | Inicializacija, senzorji, Wi-Fi, MQTT in ArduinoOTA |
| `src/app_web.cpp`, `include/app_web.h` | Spletne poti, JSON status in varno nalaganje slike |
| `src/lcd_ui.cpp`, `include/lcd_ui.h` | Ozadje v5, izpis temperatur in Wi-Fi ikona |
| `src/device_restart.cpp`, `include/device_restart.h` | Skupni odloženi ponovni zagon in blokada med pisanjem |
| `include/tft_setup.h` | Izrecna konfiguracija TFT, povzeta po dejanskem začetnem buildu |
| `web/index.html`, `web/app.js` | Produkcijska stran brez zunanjih knjižnic ali CDN |
| `web/sensors.html`, `web/sensors.js`, `include/sensor_diagnostics.h` | Bralni pregled ROM naslovov in stalnih dodelitev |
| `include/sensor_mapping.h` | Potrjena ROM preslikava za LCD, splet in MQTT |
| `data/zalogovnik_reference_v5.jpg` | Potrjeno ozadje LCD, 480 × 320, baseline JPEG |
| `scripts/embed_assets.py`, `scripts/pio_assets.py` | Determinističen izvoz gzip/PROGMEM pred buildom |
| `preview/` | Oblikovni predogledi in lokalni posnetki; niso datoteke za filesystem naprave |

Generirani `include/generated_assets.h` in `src/generated_assets.cpp` se obnovita pred buildom in sta ignorirana v Gitu. Generator uporablja samo Pythonovo standardno knjižnico. Izvornih slik in potrjenega HTML predogleda ne spreminja.

## Hardware in nastavitve

- PlatformIO environment: `esp32dev`, board `esp32dev`, framework Arduino.
- Ob pripravi 1.1.0 je lokalni build uporabljal platformo pioarduino 55.3.38 / Arduino-ESP32 3.3.8. Verzije niso bile nadgrajene s to spremembo.
- ILI9488, landscape 480 × 320, rotation 3.
- OneWire data: GPIO16.
- TFT: MISO19, MOSI23, SCLK18, CS15, DC2, RESET4; SPI 27 MHz, read 16 MHz.
- To so učinkovite nastavitve začetnega builda iz `.pio/libdeps/esp32dev/TFT_eSPI`. Kopija v `lib/TFT_eSPI/User_Setup.h` vsebuje drugačne pine in ni bila tista, ki jo je izbral začetni build.
- `include/tft_setup.h` odstrani odvisnost od ročno spremenjenih nastavitev v `.pio` in za obstoječi OneWire 2.3.7 vključi deklaracijo `GPIO`, potrebno ob Arduino-ESP32 3.x.
- Fizična vezava in prikaz novega firmware-a se preverita po odobrenem nalaganju.

### Stalna preslikava senzorjev

Vir: uporabnikov popravljen seznam fizičnih položajev. Naslovi so v vrstnem redu Dallas/OneWire, prvi bajt je družinska koda `28`, zadnji bajt CRC.

| Prikaz / MQTT | ROM naslov |
|---|---|
| Levi zgoraj / buffer1 | `28:04:87:76:E0:01:3C:E7` |
| Levi sredina / buffer2 | `28:F5:79:76:E0:01:3C:A6` |
| Levi spodaj / buffer3 | `28:EE:25:76:E0:01:3C:B1` |
| Desni sredina / buffer4 | `28:D5:DD:49:F6:DC:3C:A4` |
| Desni zgoraj / buffer5 | `28:25:79:76:E0:01:3C:62` |
| Desni spodaj / buffer6 | `28:BB:7A:76:E0:01:3C:8E` |

Branje uporablja izključno dodeljene ROM naslove. Ni samodejnega nadomestila po indeksu, če senzor manjka. Indeksi na podstrani so le informacija ob zagonu in se lahko razlikujejo po spremembi priključenih naprav; dodelitev zato ostane ista.

Obstoječa ločljivost posameznega senzorja se ohrani. MQTT topici `zalogovniki/buffer1` do `buffer6`, sistemski topici in Discovery `unique_id` ostajajo enaki. Pri nadgradnji z 1.1.2 na 1.1.3 ostaneta buffer4 in buffer5 vezana na ista ROM naslova; popravita se le položaja prikaza in njuni oznaki. Zgodovinski podatki v Home Assistantu se ne prepisujejo.

Obstoječa MQTT objava temperatur je ohranjena. LCD in novi API ločeno označita vrednosti izven območja DS18B20 ter neveljavne vrednosti kot manjkajoče; API zanje vrne `null`.

## Spletni vmesnik

Po nalaganju novega firmware-a je stran na trenutnem IP-ju ESP-ja; v tej pripravi je nastavljen `192.168.64.51`.

| Pot | Metoda | Namen |
|---|---|---|
| `/` | GET | Dashboard, gzip HTML iz flasha |
| `/app.js` | GET | JavaScript, gzip iz flasha |
| `/api/status` | GET | Trenutne meritve, starost meritve in stanje naprave |
| `/senzorji` | GET | Nova podstran za identifikacijo senzorjev |
| `/sensors.js` | GET | JavaScript podstrani senzorjev |
| `/api/sensors` | GET | ROM naslovi in trenutna preslikava šestih bufferjev |
| `/api/restart` | POST | Potrjen ponovni zagon; GET ne zažene naprave |
| `/lcd-background.jpg` | GET | Izvirna v5 za glavni spletni prikaz, neodvisno od custom slike LCD |
| `/api/lcd-image` | GET | Aktivna custom slika ali privzeta v5 |
| `/upload` | POST multipart | Menjava ozadja prek nastavitev strani |
| `/files` | GET | Obstoječi pregled datotek SPIFFS |

Status vrne `firmware`, `boot_id` (naključen identifikator tega zagona), `sampled`, `sample_age_ms`, šest `temperatures`, `wifi` (`connected`, `ssid`, `ip`, `rssi`), `mqtt_connected`, `uptime_s`, `free_heap`, `filesystem_ready`, `lcd_custom` in `ota` (`running_partition`, `next_partition`, `slot_bytes`). Gesel ne vrača.

`/api/sensors` vrne `firmware`, `devices_at_boot`, `sampled`, `sample_age_ms`, `address_mode: "rom"`, `index_origin: "boot"` in šest objektov `sensors`, urejenih po bufferjih. Vsak ima `buffer` (1–6), `index` (indeks ob zagonu ali `null`, če ROM takrat ni bil zaznan), `rom` (stalni naslov v obliki `28:…`) in `temperature` (°C ali `null`). ROM ostane prikazan tudi ob manjkajoči meritvi. Odpiranje API-ja ne sproži novih meritev ali zapisovanja nastavitev.

Slika iz nastavitev se zapiše v `/lcd-custom.jpg`; privzeta v5 ostane v aplikaciji. Stari `/zalogovnik.jpg` v SPIFFS se ob nadgradnji ne prepiše in ne izbriše, vendar ga 1.1.0 ne izbere samodejno. Izbrana slika mora ohraniti razpored merilnih polj v5. Ponovni zagon za menjavo slike ni potreben.

Endpoint `/upload` zahteva glavo `X-Requested-With: ZalogovnikiLCD`, ki jo nova stran pošlje samodejno. Če je prisoten `Origin`, se mora ujemati z gostiteljem naprave. CORS ni omogočen. To ohrani lokalno uporabo brez uvajanja nove prijave; stran je namenjena zaupanja vrednemu lokalnemu omrežju.

`POST /api/restart` zahteva isto glavo in preverjanje izvora ter telo `RESTART` (`Content-Type: text/plain`). Sprejeta zahteva vrne HTTP 202 z `ok: true` in sedanjim `boot_id`. Brez glave oziroma ob tujem izvoru vrne 403, ob napačnem telesu 400, med prenosom slike ali OTA pa 409. Med že načrtovanim restartom novega nalaganja slike ni mogoče začeti. Po 60 sekundah brez potrjenega novega zagona spletni vmesnik izpiše, da ponovni zagon ni potrjen.

### Ponovni zagon prek MQTT / Home Assistant

- Discovery topic: `homeassistant/button/zalogovniki_restart/config`, konfiguracija je retained.
- `unique_id`: `zalogovniki_restart`; ista naprava `zalogovniki_lcd`; `device_class: restart`, `entity_category: config`.
- Command topic: `zalogovniki/restart/set`; edini sprejeti payload je natančno `RESTART`, QoS 0, **retain false**. Prazen ali drugačen payload se ignorira.
- Availability topic: `zalogovniki/status`, retained `online` po povezavi, LWT `offline` ob izgubi povezave. Ta razpoložljivost je dodana za novi gumb.
- Ob vsaki novi MQTT povezavi se stari retained payload na ukaznem topicu najprej počisti, šele nato se naprava naroči nanj. S tem shranjeni ukaz ne povzroča ponavljajočih zagonov; PubSubClient 2.8 v callbacku ne poda retain zastavice.
- Zahteve med aktivnim pisanjem slike ali OTA se zavrnejo oziroma ignorirajo. MQTT ukaz je namenjen zaupanja vrednemu brokerju in njegovim obstoječim dovoljenjem.

Pogodba Discovery temelji na uradni dokumentaciji [MQTT button](https://www.home-assistant.io/integrations/button.mqtt/) in [MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery). Dejanski prikaz nove entitete v Home Assistantu se preveri po odobrenem nalaganju.

## Build

### Lokalna konfiguracija po kloniranju

V PowerShellu iz korena projekta:

```powershell
Copy-Item include/secrets.example.h include/secrets.h
Copy-Item platformio.local.example.ini platformio.local.ini
```

V `include/secrets.h` nastavi Wi-Fi, MQTT in OTA podatke. V `platformio.local.ini` nastavi IP naprave in isto OTA geslo. Datoteki sta ignorirani v Gitu; vrednosti v obeh predlogah so samo označbe. Obstoječe lokalne poverilnice so bile pred prvo objavo prestavljene v ti datoteki brez spreminjanja vrednosti.

Lokalni build ne zahteva povezave s fizičnim ESP-jem:

```powershell
pio run -e esp32dev
```

Pred prvim nalaganjem preveri nastavitve svoje plošče, vezavo in ROM naslove svojih tipal v `include/sensor_mapping.h`. Objavljena preslikava pripada temu konkretnemu projektu. Flashanje ali OTA je ločen korak.

Obstoječi PlatformIO build `esp32dev` zažene generator assetov in sestavi aplikacijo. Za samostojno obnovo assetov je na voljo `python scripts/embed_assets.py`.

Poverilnice ostajajo v ignorirani lokalni konfiguraciji. Ne prepisuj jih v dokumentacijo, teste ali javni repozitorij. Lokalni back-upi so v ignorirani mapi `.pio/backups/`. Prenesene knjižnice v `lib/`, stari predogledi, arhivi in build binarije niso vključeni v Git. Odvisnosti določa `lib_deps` v `platformio.ini`; konfiguracija TFT je v `include/tft_setup.h`.

## OTA – pred nalaganjem

**Lokalni `board_build.partitions = huge_app.csv` ima samo eno aplikacijsko particijo.** To ni dokaz, katera particijska tabela je trenutno na fizičnem ESP-ju. Pred OTA mora biti potrjena dejanska tabela in dovolj velika druga aplikacijska particija. Če je tudi na napravi samo ena aplikacijska particija, aplikacijski OTA ne more dodati druge; postopek prehoda zahteva ločen dogovor in ohranitev podatkov.

Samo aplikacijski OTA prenaša `firmware.bin`. Ne prenaša `partitions.bin`, bootloaderja ali filesystema. Za ta korak ne uporabljaj `firmware.factory.bin`, erase-flash ali uploadfs. Lokalni build s `huge_app` sam po sebi ne potrjuje, da OTA na napravi lahko uspe.

Po izrecno odobrenem nalaganju preveri verzijo 1.1.4 prek `/api/status`, vseh šest temperatur in njihovo preslikavo, ohranjen LCD, Wi-Fi ikono, spletni in MQTT ponovni zagon ter možnost naslednje OTA posodobitve.

## Preverjanje

Lokalni build in brskalniško preverjanje z nadomestnimi API odzivi sta ločena od fizičnega delovanja ESP-ja. Posnetki `preview/web-1.1.4-desktop.png` in `preview/web-1.1.4-mobile.png` uporabljajo testne podatke. Uporabnik je potrdil videz LCD pred tem popravkom. Nov spletni/MQTT restart in Discovery iz 1.1.4 še zahtevata preverjanje na fizični napravi po odobrenem nalaganju. Backup pred spremembo je v `.pio/backups/before-web-restart-1.1.4/`.
