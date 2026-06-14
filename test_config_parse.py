with open('bin/Release/config.json', 'r', encoding='utf-8') as f:
    content = f.read()

def find_object_start(s, key, from_pos=0):
    search_key = '"' + key + '"'
    kp = s.find(search_key, from_pos)
    if kp == -1: return -1
    kp += len(search_key)
    kp = s.find(':', kp)
    if kp == -1: return -1
    kp = s.find('{', kp)
    return kp

def get_string_value(s, key, pos):
    search_key = '"' + key + '"'
    kp = s.find(search_key, pos)
    if kp == -1: return '', pos
    kp += len(search_key)
    kp = s.find(':', kp)
    if kp == -1: return '', pos
    kp += 1
    while kp < len(s) and s[kp] in ' \t\n\r':
        kp += 1
    if kp >= len(s) or s[kp] != '"': return '', pos
    kp += 1
    result = ''
    while kp < len(s) and s[kp] != '"':
        if s[kp] == '\\' and kp + 1 < len(s):
            kp += 1
        result += s[kp]
        kp += 1
    pos = kp
    return result, pos

tts_pos = find_object_start(content, 'tts')
print('tts_pos:', tts_pos)
engine_val, tts_pos = get_string_value(content, 'engine', tts_pos)
print('engine:', repr(engine_val))
voice_val, tts_pos = get_string_value(content, 'voice', tts_pos)
print('voice:', repr(voice_val))

# Also test with the actual json module
import json
with open('bin/Release/config.json', 'r', encoding='utf-8') as f:
    data = json.load(f)
print('json module engine:', repr(data.get('tts', {}).get('engine', '')))
