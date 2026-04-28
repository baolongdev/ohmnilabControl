# Conversation Action Routing

This repo now includes a trigger rule catalog for automatic emotion actions:

- [conversation_emotion_triggers.yaml](E:/ACLAB/project/python/ohmnilabControl/conversation_emotion_triggers.yaml)

## Intended use

This file is designed for the conversation agent or router layer, not for the ESP32 firmware.

Recommended flow:

1. Read user utterance / transcript.
2. Score emotion intents using the trigger catalog.
3. Apply cooldown, conflict resolution, and safety gates.
4. If an intent passes threshold, call the matching MCP tool:
   - `emotion_disagree`
   - `emotion_happy`
   - `emotion_curious`
   - `emotion_excited`
   - `emotion_shy`

## Why keep this outside firmware

- Conversation parsing changes often.
- Trigger logic is easier to tune on the server side.
- The firmware should stay focused on deterministic motion execution.

## Notes

- The catalog includes Vietnamese, English, and mixed-language cases.
- It also includes suppression rules and anti-spam limits.
- Neutral or purely technical requests should not trigger emotion actions.
