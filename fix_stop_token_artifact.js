#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

// Headers to remove from GMF section
const HEADERS_TO_REMOVE = new Set([
    '<thread>',
    '<condition_variable>',
    '<mutex>',
    '<semaphore>',
    '<latch>',
    '<barrier>',
]);

function hasUtf8Bom(filePath) {
    try {
        const buffer = Buffer.alloc(3);
        const fd = fs.openSync(filePath, 'r');
        fs.readSync(fd, buffer, 0, 3);
        fs.closeSync(fd);
        return buffer[0] === 0xef && buffer[1] === 0xbb && buffer[2] === 0xbf;
    } catch (e) {
        return false;
    }
}

function findExportModuleLine(lines) {
    for (let i = 0; i < lines.length; i++) {
        if (lines[i].includes('export module ')) {
            return i;
        }
    }
    return -1;
}

function shouldRemoveInclude(line) {
    const stripped = line.trimLeft();
    if (!stripped.startsWith('#include')) {
        return false;
    }
    const match = stripped.match(/#include\s+(<[^>]+>)/);
    if (!match) {
        return false;
    }
    return HEADERS_TO_REMOVE.has(match[1]);
}

function processFile(filePath) {
    try {
        const hasBom = hasUtf8Bom(filePath);
        let content = fs.readFileSync(filePath, 'utf-8');
        
        // Remove BOM if present
        if (content.charCodeAt(0) === 0xfeff) {
            content = content.substring(1);
        }
        
        const lines = content.split('\n');
        const exportLineIdx = findExportModuleLine(lines);
        
        if (exportLineIdx === -1) {
            return { modified: false, removed: [] };
        }
        
        const removedHeaders = [];
        const newGmfLines = [];
        
        for (let i = 0; i < exportLineIdx; i++) {
            const line = lines[i];
            if (shouldRemoveInclude(line)) {
                const match = line.match(/<([^>]+)>/);
                if (match) {
                    removedHeaders.push(match[1]);
                }
            } else {
                newGmfLines.push(line);
            }
        }
        
        const modified = removedHeaders.length > 0;
        
        if (modified) {
            const newContent = (hasBom ? '\ufeff' : '') + 
                             newGmfLines.join('\n') + '\n' +
                             lines.slice(exportLineIdx).join('\n');
            fs.writeFileSync(filePath, newContent, 'utf-8');
        }
        
        return { modified, removed: removedHeaders };
    } catch (e) {
        console.error(`ERROR processing ${filePath}: ${e.message}`);
        return { modified: false, removed: [] };
    }
}

function walkDir(dir, callback) {
    fs.readdirSync(dir, { withFileTypes: true }).forEach(entry => {
        const fullPath = path.join(dir, entry.name);
        if (entry.isDirectory()) {
            walkDir(fullPath, callback);
        } else if (entry.isFile() && entry.name.endsWith('.ixx')) {
            callback(fullPath);
        }
    });
}

function main() {
    const baseDir = 'X:\\Dev\\ArtifactStudio\\Artifact\\include';
    
    if (!fs.existsSync(baseDir)) {
        console.log(`ERROR: Directory not found: ${baseDir}`);
        return;
    }
    
    console.log(`Scanning for .ixx files in: ${baseDir}\n`);
    
    const ixxFiles = [];
    walkDir(baseDir, (file) => ixxFiles.push(file));
    
    if (ixxFiles.length === 0) {
        console.log('No .ixx files found.');
        return;
    }
    
    console.log(`Found ${ixxFiles.length} .ixx files\n`);
    
    const modifiedFiles = [];
    const totalRemoved = {};
    
    ixxFiles.sort().forEach(filePath => {
        const result = processFile(filePath);
        
        if (result.modified) {
            const relPath = path.relative(baseDir, filePath);
            modifiedFiles.push([relPath, result.removed]);
            console.log(`✓ Modified: ${relPath}`);
            result.removed.forEach(header => {
                totalRemoved[header] = (totalRemoved[header] || 0) + 1;
                console.log(`  - Removed: #include <${header}>`);
            });
        }
    });
    
    console.log('\n' + '='.repeat(60));
    console.log('SUMMARY');
    console.log('='.repeat(60));
    console.log(`Total files processed: ${ixxFiles.length}`);
    console.log(`Files modified: ${modifiedFiles.length}`);
    
    if (Object.keys(totalRemoved).length > 0) {
        console.log('\nHeaders removed:');
        Object.keys(totalRemoved).sort().forEach(header => {
            console.log(`  <${header}>: ${totalRemoved[header]} occurrence(s)`);
        });
    } else {
        console.log('\nNo headers were removed.');
    }
}

main();
