import os
import sys
import json
import time
import uuid

BRIDGE_DIR = r"E:\UE\Tank\Bridge"
QUEUE_DIR = os.path.join(BRIDGE_DIR, "queue")
RESULTS_DIR = os.path.join(BRIDGE_DIR, "results")

def run_ue_py(cmd_text, timeout=120):
    task_id = str(uuid.uuid4())[:8]
    queue_file = os.path.join(QUEUE_DIR, f"{task_id}.json")
    result_file = os.path.join(RESULTS_DIR, f"{task_id}.json")
    
    payload = {
        "type": "py",
        "command": cmd_text
    }
    
    with open(queue_file, "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False)
        
    start_time = time.time()
    while time.time() - start_time < timeout:
        if os.path.exists(result_file):
            try:
                with open(result_file, "r", encoding="utf-8") as f:
                    res = json.load(f)
                os.remove(result_file)
                return res
            except Exception:
                pass
        time.sleep(0.2)
        
    raise TimeoutError(f"Command timed out after {timeout} seconds")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        code = sys.argv[1]
    else:
        code = "out.append(f'World: {unreal.EditorLevelLibrary.get_editor_world().get_name()}')"
    res = run_ue_py(code)
    print(json.dumps(res, ensure_ascii=False, indent=2))
