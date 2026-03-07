import os
import re

directories = ['ArtifactCore/src', 'ArtifactCore/include', 'ArtifactWidgets/src', 'ArtifactWidgets/include', 'Artifact/src', 'Artifact/include']

includes = '''
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
'''

for directory in directories:
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith('.cpp') or file.endswith('.cppm') or file.endswith('.ixx'):
                filepath = os.path.join(root, file)
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                if 'import std;' in content:
                    # Remove 'import std;' and insert includes right after 'module;' or 'export module ...;'
                    new_content = re.sub(r'import\s+std\s*;', '', content)
                    
                    if 'export module' in new_content:
                        new_content = re.sub(r'(export\s+module\s+[a-zA-Z0-9\._:]+\s*;)', r'\1\n' + includes, new_content, count=1)
                    elif 'module' in new_content:
                        # Find the first module declaration that is not 'module;'
                        match = re.search(r'module\s+([a-zA-Z0-9\._:]+)\s*;', new_content)
                        if match:
                            new_content = re.sub(r'(module\s+[a-zA-Z0-9\._:]+\s*;)', r'\1\n' + includes, new_content, count=1)
                        else:
                            new_content = re.sub(r'(module\s*;)', r'\1\n' + includes, new_content, count=1)
                    
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(new_content)
                    print(f"Updated {filepath}")
