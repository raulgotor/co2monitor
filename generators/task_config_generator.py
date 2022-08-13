import yaml


def get_yaml():
    with open("main/tasks.yaml") as f:
        return yaml.safe_load(f)

def generate():
    for task in get_yaml():
        a = Task(name=task['task_name'],
             depth=task['depth'],
             priority=task['priority'],
                 rate_ms=task['rate_ms'])
        print(a.get_define_str())

class Task:
    def __init__(self, name, depth, priority, rate_ms):
        self.name = name
        self.depth = depth
        self.priority = priority
        self.rate_ms = rate_ms

    def get_define_str(self):
        return f"TASKS_CONFIG_{self.name.upper()}_STACK_DEPTH"


generate()
