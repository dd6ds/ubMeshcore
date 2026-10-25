#!/usr/bin/env python3
"""Fills i18n/ubmeshcore_de.ts with the German translations below.

Workflow when texts change:
  1. lupdate (see README, "Translations") to refresh the .ts file
  2. add the new English texts to DE below
  3. python3 i18n/translate_de.py   - fails and lists any text still missing

MeshCore / radio terms (Advert, Repeater, Flood, SNR, RSSI, Hop) are kept as
German MeshCore users say them.
"""
import sys
import xml.etree.ElementTree as ET

DE = {
    # --- connection / Bluetooth -------------------------------------------
    "Session started": "Sitzung gestartet",
    "Session started - ": "Sitzung gestartet - ",
    "Battery: %1% (%2 V)": "Akku: %1% (%2 V)",
    "Checking Bluetooth...": "Bluetooth wird geprüft...",
    "Scan finished - %1 device(s)": "Suche beendet - %1 Gerät(e)",
    "No Bluetooth adapter found": "Kein Bluetooth-Adapter gefunden",
    "Bluetooth is switched off": "Bluetooth ist ausgeschaltet",
    "Ready": "Bereit",
    "Discovery not supported on this device": "Gerätesuche wird auf diesem Gerät nicht unterstützt",
    "Scanning...": "Suche läuft...",
    "Scan stopped": "Suche gestoppt",
    "Scan finished - no devices found": "Suche beendet - keine Geräte gefunden",
    "Bluetooth I/O error": "Bluetooth-E/A-Fehler",
    "Invalid Bluetooth adapter": "Ungültiger Bluetooth-Adapter",
    "Discovery unsupported on this platform": "Gerätesuche auf dieser Plattform nicht unterstützt",
    "Requested discovery method unsupported": "Angeforderte Suchmethode nicht unterstützt",
    "Error: ": "Fehler: ",
    "No device selected": "Kein Gerät ausgewählt",
    "Unknown device - please scan again": "Unbekanntes Gerät - bitte erneut suchen",
    "Connecting...": "Verbinde...",
    "Connected - discovering services...": "Verbunden - Dienste werden gesucht...",
    "Not connected": "Nicht verbunden",
    "Connected, but no Nordic UART service": "Verbunden, aber kein Nordic-UART-Dienst",
    "Could not open UART service": "UART-Dienst konnte nicht geöffnet werden",
    "Reading UART characteristics...": "UART-Merkmale werden gelesen...",
    "UART characteristics missing": "UART-Merkmale fehlen",
    "UART ready - starting session...": "UART bereit - Sitzung wird gestartet...",
    "Not connected to a node": "Mit keinem Node verbunden",
    "Node session not started yet": "Node-Sitzung noch nicht gestartet",
    "Could not enable notifications": "Benachrichtigungen konnten nicht aktiviert werden",
    "Connection failed: ": "Verbindung fehlgeschlagen: ",
    "Unknown node": "Unbekannter Node",
    "Unknown contact": "Unbekannter Kontakt",

    # --- offline queue / advert --------------------------------------------
    "%1 message(s) fetched from node at %2": "%1 Nachricht(en) um %2 vom Node abgerufen",
    "Advert failed: ": "Advert fehlgeschlagen: ",
    "%1 advert sent at %2": "%1-Advert um %2 gesendet",
    "Flood": "Flood",
    "Zero-hop": "Zero-Hop",
    "No answer from the node - advert may not have been sent":
        "Keine Antwort vom Node - Advert wurde evtl. nicht gesendet",
    "Connection lost before the node confirmed the advert":
        "Verbindung getrennt, bevor der Node das Advert bestätigt hat",
    "Advert not sent - not connected to your node":
        "Advert nicht gesendet - nicht mit deinem Node verbunden",
    "Sending advert...": "Advert wird gesendet...",

    # --- repeater status ----------------------------------------------------
    "%1 d %2 h": "%1 T %2 Std.",
    "%1 h %2 min": "%1 Std. %2 Min.",
    "%1 min": "%1 Min.",
    "Battery": "Akku",
    "Uptime": "Laufzeit",
    "Noise floor": "Grundrauschen",
    "TX queue": "Sende-Warteschlange",
    "Packets rx / tx": "Pakete empfangen / gesendet",
    "TX airtime": "Sendezeit (TX)",
    "Flood rx / tx": "Flood empfangen / gesendet",
    "Direct rx / tx": "Direkt empfangen / gesendet",
    "Last SNR": "Letzter SNR",
    "Errors": "Fehler",
    "Duplicates direct / flood": "Duplikate direkt / Flood",
    "RX airtime": "Empfangszeit (RX)",
    "Raw": "Rohdaten",

    # --- hop counts in the contact list (UI) ---------------------------------
    # The message route itself is NOT translated: it is part of the message
    # (history, "Reply with route") - see BluetoothManager::routeInfo().
    "direct": "direkt",
    "1 hop": "1 Hop",
    "%1 hops": "%1 Hops",

    # --- channels -----------------------------------------------------------
    '"%1" is not a valid channel name': '„%1“ ist kein gültiger Kanalname',
    "Channel slots not read yet - try again in a moment":
        "Kanalplätze noch nicht gelesen - gleich noch einmal versuchen",
    "All %1 channel slots are in use - remove one first":
        "Alle %1 Kanalplätze sind belegt - erst einen entfernen",
    "No channel in that slot": "Kein Kanal auf diesem Platz",

    # --- node errors ----------------------------------------------------------
    "unsupported command": "nicht unterstützter Befehl",
    "not found": "nicht gefunden",
    "table full": "Tabelle voll",
    "bad state": "ungültiger Zustand",
    "storage error": "Speicherfehler",
    "illegal argument": "ungültiges Argument",
    "Node rejected the last command": "Der Node hat den letzten Befehl abgelehnt",
    "Node rejected the last command (%1)": "Der Node hat den letzten Befehl abgelehnt (%1)",

    # --- contacts page ----------------------------------------------------------
    "Repeater": "Repeater",
    "Room": "Raum",
    "Companion": "Companion",
    "Sensor": "Sensor",
    "Unknown": "Unbekannt",
    "flood": "Flood",
    "Connect to your node to load contacts": "Mit deinem Node verbinden, um Kontakte zu laden",
    "Loading contacts...": "Kontakte werden geladen...",
    "%1 contact(s) from received adverts": "%1 Kontakt(e) aus empfangenen Adverts",
    "Refresh": "Aktualisieren",
    "Advert": "Advert",
    " - nodes and repeaters in range will list you after receiving it.":
        " - Nodes und Repeater in Reichweite listen dich, sobald sie es empfangen.",
    "Search name or key...": "Name oder Schlüssel suchen...",
    "All": "Alle",
    "Repeaters": "Repeater",
    "Companions": "Companions",
    "Rooms": "Räume",
    "★ Only favourites": "★ Nur Favoriten",
    "Only active (24 h)": "Nur aktive (24 Std.)",
    "No favourites yet. Tap the ☆ on a contact to mark it.":
        "Noch keine Favoriten. Tippe auf ☆ bei einem Kontakt, um ihn zu markieren.",
    'No contacts yet. Repeaters and nodes appear here as soon as your node receives their adverts. Tap "Advert" to announce yourself.':
        "Noch keine Kontakte. Repeater und Nodes erscheinen hier, sobald dein Node ihre Adverts "
        "empfängt. Tippe auf „Advert“, um dich bekannt zu machen.",
    "Manage ›": "Verwalten ›",
    "never": "nie",
    "just now": "gerade eben",
    "%1 s ago": "vor %1 s",
    "%1 min ago": "vor %1 Min.",
    "%1 h ago": "vor %1 Std.",
    "%1 d ago": "vor %1 T",

    # --- repeater admin -----------------------------------------------------------
    'Waiting for reply to "%1"...': "Warte auf Antwort auf „%1“...",
    "! no reply (timeout) - %1": "! keine Antwort (Zeitüberschreitung) - %1",
    "Logged in as admin": "Als Admin angemeldet",
    "Logged in as guest (read-only)": "Als Gast angemeldet (nur lesen)",
    "Login failed - wrong password or no reply": "Anmeldung fehlgeschlagen - falsches Passwort oder keine Antwort",
    "Status received": "Status empfangen",
    "‹ Contacts": "‹ Kontakte",
    "Connected, node session still starting... (%1)": "Verbunden, Node-Sitzung startet noch... (%1)",
    "Not connected to your node. Go to the device page and connect again, then come back.":
        "Nicht mit deinem Node verbunden. Auf der Geräteseite neu verbinden und dann zurückkommen.",
    "Login": "Anmelden",
    "Admin password": "Admin-Passwort",
    "Re-login": "Neu anmelden",
    "Logging in...": "Anmeldung läuft...",
    "Login request sent": "Anmeldeanfrage gesendet",
    "Admin session active - CLI commands enabled": "Admin-Sitzung aktiv - CLI-Befehle freigeschaltet",
    "Guest login - status only, CLI needs the admin password":
        "Gast-Anmeldung - nur Status, für CLI wird das Admin-Passwort benötigt",
    "Not logged in. Leave empty for guest access if the node allows it.":
        "Nicht angemeldet. Für Gastzugang leer lassen, falls der Node das erlaubt.",
    "Status": "Status",
    "Request status": "Status abfragen",
    "Requesting status...": "Status wird abgefragt...",
    "No status yet. Most repeaters answer status requests only after a login.":
        "Noch kein Status. Die meisten Repeater antworten erst nach einer Anmeldung.",
    "Commands": "Befehle",
    "Version": "Version",
    "Clock": "Uhrzeit",
    "Sync clock": "Uhr stellen",
    "Neighbours": "Nachbarn",
    "Name": "Name",
    "Radio": "Funk",
    "TX power": "Sendeleistung",
    "Send advert": "Advert senden",
    "CLI command, e.g. set name MyRepeater": "CLI-Befehl, z. B. set name MeinRepeater",
    "Log in as admin to send commands": "Als Admin anmelden, um Befehle zu senden",
    "Send": "Senden",

    # --- direct messages / contact menu ---------------------------------------------
    "sending...": "wird gesendet...",
    "sent - waiting for confirmation": "gesendet - warte auf Bestätigung",
    "✓ delivered": "✓ zugestellt",
    "⚠ no confirmation received": "⚠ keine Bestätigung erhalten",
    "Direct message": "Direktnachricht",
    "Not connected to your node": "Nicht mit deinem Node verbunden",
    "No messages with %1 yet.": "Noch keine Nachrichten mit %1.",
    "Message to %1...": "Nachricht an %1...",
    "Send message": "Nachricht senden",
    "Manage": "Verwalten",
    "Remove from favourites": "Aus Favoriten entfernen",
    "Add to favourites": "Zu Favoriten hinzufügen",
    "Chat ›": "Chat ›",
    "Save to node": "Auf Node speichern",
    "Delete contact": "Kontakt löschen",
    "Delete contacts": "Kontakte löschen",
    "Delete %1?": "%1 löschen?",
    "Delete %1 contacts?": "%1 Kontakte löschen?",
    "They are removed from your node and from this list. A contact shows up again the next time its advert is heard.":
        "Sie werden von deinem Node und aus dieser Liste entfernt. Ein Kontakt erscheint wieder, "
        "sobald sein Advert erneut empfangen wird.",
    "Done": "Fertig",
    "Select": "Auswählen",
    "Tap contacts to select them": "Kontakte zum Auswählen antippen",
    "%1 selected": "%1 ausgewählt",
    "Select ▾": "Auswahl ▾",
    "All except favourites": "Alle außer Favoriten",
    "Inactive for 7 days": "7 Tage inaktiv",
    "Inactive for 30 days": "30 Tage inaktiv",
    "All (including favourites)": "Alle (inkl. Favoriten)",
    "Clear selection": "Auswahl aufheben",
    "Delete": "Löschen",
    "%1 contact(s) · %2 of %3 stored on your node": "%1 Kontakt(e) · %2 von %3 auf deinem Node gespeichert",
    "not on node": "nicht auf dem Node",
    "%1 is not stored on your node yet. Sending a message saves it there first.":
        "%1 ist noch nicht auf deinem Node gespeichert. Beim Senden wird der Kontakt zuerst dort gespeichert.",
    "Your node's contact list is full (%1 of %2). Remove a contact first: long-press it in the contact list → Delete contact.":
        "Die Kontaktliste deines Nodes ist voll (%1 von %2). Entferne zuerst einen Kontakt: "
        "in der Kontaktliste lange drücken → Kontakt löschen (oder „Auswählen“ für mehrere).",

    # --- main pages -----------------------------------------------------------------
    "Discover MeshCore Nodes": "MeshCore-Nodes suchen",
    "Scan for Bluetooth devices": "Nach Bluetooth-Geräten suchen",
    "Stop Scan": "Suche stoppen",
    "Start Scan": "Suche starten",
    "Connected to: %1": "Verbunden mit: %1",
    "%1 device(s) found": "%1 Gerät(e) gefunden",
    "Classic": "Classic",
    "Connect to Node": "Mit Node verbinden",
    "Selected Node: %1": "Ausgewählter Node: %1",
    "None": "Keiner",
    "Disconnect": "Trennen",
    "Connect": "Verbinden",
    "Node: %1": "Node: %1",
    "(unnamed)": "(ohne Namen)",
    "Channels": "Kanäle",
    "Contacts": "Kontakte",
    "Back": "Zurück",
    "MeshCore Channels": "MeshCore-Kanäle",
    "not connected": "nicht verbunden",
    "Channel name, e.g. #p53": "Kanalname, z. B. #p53",
    "Open %1": "%1 öffnen",
    "Pick a channel": "Kanal wählen",
    "Invalid channel name": "Ungültiger Kanalname",
    "Add %1": "%1 hinzufügen",
    "Waiting for the channel list from the node...": "Warte auf die Kanalliste vom Node...",
    "Channels on %1 (%2 of %3 slots used):": "Kanäle auf %1 (%2 von %3 Plätzen belegt):",
    "Channels on %1:": "Kanäle auf %1:",
    "1 new message": "1 neue Nachricht",
    "%1 new messages": "%1 neue Nachrichten",
    "Remove channel": "Kanal entfernen",
    "Remove %1 from your node?\n\nIts message history in the app is deleted too. You can add the channel again later by typing its name.":
        "%1 von deinem Node entfernen?\n\nDer Nachrichtenverlauf in der App wird ebenfalls "
        "gelöscht. Du kannst den Kanal später wieder hinzufügen, indem du seinen Namen eingibst.",
    "Messages - %1": "Nachrichten - %1",
    "No Channel": "Kein Kanal",
    "‹ Channels": "‹ Kanäle",
    "No channel": "Kein Kanal",
    "session ready": "Sitzung bereit",
    "no node session": "keine Node-Sitzung",
    "Enter message...": "Nachricht eingeben...",
    "Nothing typed yet": "Noch nichts eingegeben",
    "No node session - connect first": "Keine Node-Sitzung - zuerst verbinden",
    "No channel selected": "Kein Kanal ausgewählt",
    "No messages in this channel yet": "Noch keine Nachrichten in diesem Kanal",
    "Reply to %1": "%1 antworten",
    "Reply with route": "Mit Route antworten",
}

TS = "i18n/ubmeshcore_de.ts"
tree = ET.parse(TS)
missing = []
for msg in tree.iter("message"):
    src = msg.find("source").text
    tr = msg.find("translation")
    if src in DE:
        tr.text = DE[src]
        tr.attrib.pop("type", None)        # drop type="unfinished"
    else:
        missing.append(src)

if missing:
    print("Missing German translations:")
    for m in sorted(set(missing)):
        print("  ", repr(m))
    sys.exit(1)

tree.write(TS, encoding="utf-8", xml_declaration=True)
print(f"{TS}: all texts translated")
