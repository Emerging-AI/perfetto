import json
import os
import shlex

# Function to convert relative paths to absolute paths
def convert_to_absolute_path(base_directory, relative_path):
    return os.path.realpath(os.path.join(base_directory, relative_path))

# Load the existing compile_commands.json file
with open('compile_commands.json', 'r') as f:
    compile_commands = json.load(f)

# Iterate over each entry and convert relative paths to absolute paths
for entry in compile_commands:
    command = entry.get('command', '')
    if command:
        # Use shlex.split to safely split the command string into arguments
        arguments = shlex.split(command)
        entry['arguments'] = arguments
    del entry['command']
    
    
    directory = entry.get('directory', '')
    if directory:
        # Convert the 'file' field to an absolute path
        file_path = entry.get('file', '')
        if file_path:
            entry['file'] = convert_to_absolute_path(directory, file_path)
        
        # Convert all paths in 'arguments' field to absolute paths
        arguments = entry.get('arguments', [])
        for i, arg in enumerate(arguments):
            if arg and (arg.endswith('.cc') or arg.endswith('.h') or arg.endswith('.cpp')):  # Check if it's a path
                arguments[i] = convert_to_absolute_path(directory, arg)
            if arg.startswith("-I") and "../" in arg:
                arguments[i] = "-I" + convert_to_absolute_path(directory, arg[2:])
            elif arg.startswith("-I") and arg[2] != '/' in arg:
                arguments[i] = "-I" + convert_to_absolute_path(directory, arg[2:])
            elif "../" in arg and arguments[i-1] == '-isystem':
                arguments[i] = convert_to_absolute_path(directory, arg)


        entry['arguments'] = arguments
    

# Save the updated compile_commands.json file
with open('compile_commands_fixed.json', 'w') as f:
    json.dump(compile_commands, f, indent=2)

print("Updated compile_commands.json with absolute paths saved as 'compile_commands_fixed.json'")

