#!/usr/bin/env python3
import sys, queue, json, time, random
import sounddevice as sd
import vosk
import os

HOTWORD = "jarvis"   # customize hotword
ACTIVE_TIMEOUT = 2   # seconds after hotword to stay active

q = queue.Queue()

def callback(indata, frames, time_info, status):
    # if status:
    #     print(status, file=sys.stderr)
    q.put(bytes(indata))

def main():
    # Set environment variables to suppress logs
    os.environ["VOSK_LOG_LEVEL"] = "0"  # 0 = no logs, 1 = errors, 2 = warnings, 3 = info
    os.environ["KALDI_LOG_LEVEL"] = "0"  # Completely silence Kaldi logs
    
    # Jarvis response messages in Iron Man style
    jarvis_responses = [
        "I'm listening sir.",
        "At your service, sir.",
        "How may I assist you today, sir?",
        "Ready and waiting, sir.",
        "Processing your request, sir.",
        "Standing by for instructions, sir.",
        "I'm all ears, sir.",
        "What can I do for you, sir?",
        "Awaiting your instructions, sir.",
        "How can I be of assistance, sir?",
    ]
    
    # Additional settings to silence all Vosk/Kaldi logs
    if hasattr(vosk, "SetLogLevel"):
        vosk.SetLogLevel(-1)  # Set to lowest possible level
    
    # Redirect stderr temporarily during model loading to suppress logs
    original_stderr = sys.stderr
    sys.stderr = open(os.devnull, 'w')
    
    model_path = os.path.expanduser("~/.config/cjsh/Jarvis/vosk-model-small-en-us-0.15")
    model = vosk.Model(model_path)
    rec = vosk.KaldiRecognizer(model, 16000)
    
    # Restore stderr
    sys.stderr.close()
    sys.stderr = original_stderr

    active = False
    last_active = 0

    with sd.RawInputStream(samplerate=16000, blocksize=8000,
                           dtype="int16", channels=1,
                           callback=callback):
        #print("[system] Ready. Say 'jarvis' to wake me up.", file=sys.stderr)
        while True:
            data = q.get()

            if rec.AcceptWaveform(data):
                result = json.loads(rec.Result())
                if "text" in result:
                    text = result["text"].strip().lower()
                    if not text:
                        continue

                    if not active and HOTWORD in text:
                        # Select a random response when Jarvis is activated
                        response = random.choice(jarvis_responses)
                        print(f"\n[jarvis] {response}", file=sys.stderr)
                        active = True
                        last_active = time.time()
                        continue

                    if active:
                        # Only print the actual command to stdout (no prefixes)
                        # This will be treated as a command to execute
                        print(text)  # forward command
                        sys.stdout.flush()
                        last_active = time.time()  # reset timeout
            else:
                # Handle partial results (streaming speech)
                part = json.loads(rec.PartialResult())
                if "partial" in part:
                    text = part["partial"].strip().lower()
                    if active and text:
                        # optional: print partials for real-time feedback
                        # print(f"(partial) {text}", file=sys.stderr)
                        last_active = time.time()

            # Timeout handling
            if active and (time.time() - last_active > ACTIVE_TIMEOUT):
                active = False
                #print("[hotword] timeout, listening again", file=sys.stderr)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
