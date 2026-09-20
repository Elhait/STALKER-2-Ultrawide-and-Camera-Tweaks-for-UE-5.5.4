using CUE4Parse;
using CUE4Parse.FileProvider;
using CUE4Parse.MappingsProvider;
using CUE4Parse.MappingsProvider.Usmap;
using CUE4Parse.UE4.Versions;
using Serilog;

Log.Logger = new LoggerConfiguration().MinimumLevel.Debug().WriteTo.Console().CreateLogger();
CUE4ParseLog.UseLogger(Log.Logger);

if (args.Length < 2)
{
    Console.Error.WriteLine("Usage: CUE4ParseWVF <game-root> <additional-container-directory> [usmap] [target-package]");
    return 2;
}

var root = Path.GetFullPath(args[0]);
var additional = new DirectoryInfo(Path.GetFullPath(args[1]));
var targetPackage = args.Length >= 4 ? args[3] : "/Weapon_Viewmodel_FOV/WVF.uasset";
var targetSuffix = targetPackage.TrimStart('/');
var utoc = additional.GetFiles("*N.utoc").SingleOrDefault();
Console.WriteLine($"Root: {root}");
Console.WriteLine($"Target package: {targetPackage}");
var provider = new DefaultFileProvider(new DirectoryInfo(root), new[] { additional }, SearchOption.AllDirectories, true, new VersionContainer(EGame.GAME_UE5_5));
// Blueprint bytecode is not part of the default lightweight package read.
// Keep this explicit for the targeted WVF Kismet recovery batch.
provider.ReadScriptData = true;
provider.UseLazyPackageSerialization = false;
Console.WriteLine($"ReadScriptData={provider.ReadScriptData} UseLazyPackageSerialization={provider.UseLazyPackageSerialization}");
Console.WriteLine("Public methods: " + string.Join(" | ", typeof(DefaultFileProvider).GetMethods().Where(m => m.IsPublic).Select(m => m.ToString()).Distinct().OrderBy(n => n).Take(80)));
Console.WriteLine("Constructors: " + string.Join(" | ", typeof(DefaultFileProvider).GetConstructors().Select(c => c.ToString())));
Console.WriteLine("IoStore types: " + string.Join(", ", typeof(DefaultFileProvider).Assembly.GetTypes().Where(t => t.Name.Contains("IoStore", StringComparison.OrdinalIgnoreCase) || t.Name.Contains("IoPackage", StringComparison.OrdinalIgnoreCase)).Select(t => t.FullName).OrderBy(n => n)));
var ioType = typeof(CUE4Parse.UE4.IO.IoStoreReader);
Console.WriteLine("IoStoreReader constructors: " + string.Join(" | ", ioType.GetConstructors().Select(c => c.ToString())));
Console.WriteLine("IoStoreReader methods: " + string.Join(" | ", ioType.GetMethods().Where(m => m.DeclaringType == ioType).Select(m => m.ToString()).OrderBy(n => n)));
Console.WriteLine("Toc options: " + string.Join(", ", Enum.GetNames(typeof(CUE4Parse.UE4.IO.Objects.EIoStoreTocReadOptions))));
var globalProperty = provider.GetType().GetProperty("GlobalData", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic);

string DescribeKismetValue(object? value, CUE4Parse.UE4.Assets.IoPackage? package = null, int depth = 0)
{
    if (value is null)
        return "<null>";
    if (value is string || value.GetType().IsPrimitive || value.GetType().IsEnum)
        return value.ToString() ?? "<empty>";
    if (value is CUE4Parse.UE4.Objects.UObject.FPackageIndex packageIndex && package is not null)
    {
        try
        {
            var resolved = package.ResolvePackageIndex(packageIndex);
            if (packageIndex.Index < 0 && resolved.ToString() == string.Empty)
            {
                var importMapField = package.GetType().GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public)
                    .FirstOrDefault(field => field.Name.Contains("ImportMap", StringComparison.OrdinalIgnoreCase));
                if (importMapField?.GetValue(package) is Array importMap)
                {
                    var importIndex = -packageIndex.Index - 1;
                    if (importIndex >= 0 && importIndex < importMap.Length && importMap.GetValue(importIndex) is CUE4Parse.UE4.IO.Objects.FPackageObjectIndex importObjectIndex)
                        resolved = package.ResolveObjectIndex(importObjectIndex);
                }
            }
            return $"FPackageIndex({packageIndex})=>{resolved}";
        }
        catch (Exception resolveException)
        {
            return $"FPackageIndex({packageIndex})=><{resolveException.GetType().Name}>";
        }
    }
    if (value is CUE4Parse.UE4.IO.Objects.FPackageObjectIndex objectIndex && package is not null)
    {
        try
        {
            var resolved = package.ResolveObjectIndex(objectIndex);
            var typeAndId = GetNamedMemberValue(objectIndex, "TypeAndId");
            var packageRef = objectIndex.IsPackageImport ? $"PackageImportIndex={objectIndex.AsPackageImportRef.ImportedPackageIndex}, HashIndex={objectIndex.AsPackageImportRef.ImportedPublicExportHashIndex}" : "";
            return $"FPackageObjectIndex(Type={objectIndex.Type}, TypeAndId={typeAndId}, {packageRef})=>{resolved}";
        }
        catch (Exception resolveException)
        {
            var typeAndId = GetNamedMemberValue(objectIndex, "TypeAndId");
            var packageRef = objectIndex.IsPackageImport ? $"PackageImportIndex={objectIndex.AsPackageImportRef.ImportedPackageIndex}, HashIndex={objectIndex.AsPackageImportRef.ImportedPublicExportHashIndex}" : "";
            return $"FPackageObjectIndex(Type={objectIndex.Type}, TypeAndId={typeAndId}, {packageRef})=><{resolveException.GetType().Name}>";
        }
    }
    if (value is Array array)
    {
        var items = Enumerable.Range(0, Math.Min(array.Length, 4))
            .Select(index => DescribeKismetValue(array.GetValue(index), package, depth + 1));
        return $"Array[{array.Length}]{{{string.Join(", ", items)}}}";
    }
    if (depth >= 4)
        return value.GetType().Name;

    var fields = value.GetType().GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic)
        .Where(f => !f.IsStatic)
        .Take(8)
        .Select(field =>
        {
            try
            {
                return $"{field.Name}={DescribeKismetValue(field.GetValue(value), package, depth + 1)}";
            }
            catch (Exception detailException)
            {
                return $"{field.Name}=<{detailException.GetType().Name}>";
            }
        });
    return $"{value.GetType().Name}{{{string.Join(", ", fields)}}}";
}

object? GetNamedMemberValue(object value, string name)
{
    var field = value.GetType().GetField(name, System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic);
    if (field is not null)
        return field.GetValue(value);
    var property = value.GetType().GetProperty(name, System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic);
    return property?.GetValue(value);
}

if (args.Length >= 3)
{
    var mappingPath = Path.GetFullPath(args[2]);
    provider.MappingsContainer = new FileUsmapTypeMappingsProvider(mappingPath);
    Console.WriteLine($"Mappings: {mappingPath}");
}
provider.Initialize();
Console.WriteLine($"Files indexed: {provider.Files.Count}");

if (utoc is not null)
{
    // Register the research container with the provider so package-import
    // resolution can use the provider's FPackageId map, not only the direct
    // reader's container header.
    provider.RegisterVfs(utoc.FullName);
    provider.Mount();
    Console.WriteLine($"Provider VFS after explicit register: mounted={provider.MountedVfs.Count} filesById={provider.FilesById.Count}");
    var mpcCandidates = provider.Files.Values
        .Where(file => file.Path.Contains("MPC_FOV", StringComparison.OrdinalIgnoreCase))
        .Select(file => file.Path)
        .Distinct(StringComparer.OrdinalIgnoreCase)
        .OrderBy(path => path)
        .Take(50)
        .ToArray();
    Console.WriteLine($"MPC_FOV indexed candidates: {mpcCandidates.Length}");
    foreach (var mpcCandidate in mpcCandidates)
        Console.WriteLine($"MPC_FOV INDEXED: {mpcCandidate}");
    Console.WriteLine("Provider reverse-reference members: " + string.Join(" | ", typeof(DefaultFileProvider).GetMethods().Where(m => m.Name.Contains("Ref", StringComparison.OrdinalIgnoreCase) || m.Name.Contains("Depend", StringComparison.OrdinalIgnoreCase)).Select(m => m.ToString()).OrderBy(n => n)));
    using var reader = new CUE4Parse.UE4.IO.IoStoreReader(utoc.FullName, CUE4Parse.UE4.IO.Objects.EIoStoreTocReadOptions.ReadAll, new VersionContainer(EGame.GAME_UE5_5));
    Console.WriteLine($"Direct IoStoreReader: mount={reader.MountPoint} encrypted={reader.IsEncrypted} directoryIndex={reader.HasDirectoryIndex}");
    Console.WriteLine($"IoStoreReader interfaces: {string.Join(" | ", reader.GetType().GetInterfaces().Select(i => i.FullName))}");
    Console.WriteLine($"IVfsFileProvider methods: {string.Join(" | ", typeof(CUE4Parse.FileProvider.Vfs.IVfsFileProvider).GetMethods().Select(m => m.ToString()).OrderBy(n => n))}");
    var chunkCountField = ioType.GetField("_packageDataChunkCount", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
    Console.WriteLine($"Package data chunk count={chunkCountField?.GetValue(reader)}");
    reader.Mount(StringComparer.OrdinalIgnoreCase);
    Console.WriteLine($"After Mount: mount={reader.MountPoint} packages={reader.PackageIdIndex.Count}");
    Console.WriteLine($"PackageIdIndex type={reader.PackageIdIndex?.GetType().FullName}");
    if (reader.PackageIdIndex is System.Collections.IEnumerable ids)
        foreach (var id in ids)
            Console.WriteLine($"PACKAGE {id}");
    var target = reader.PackageIdIndex.FirstOrDefault(kvp => kvp.Value.Path.EndsWith(targetSuffix, StringComparison.OrdinalIgnoreCase));
    if (target.Value is not null)
    {
        Console.WriteLine($"Target package: packageId={target.Key.id} path={target.Value.Path}");
        // Batch 3.1 diagnostic: compare the normal provider load path with the
        // manually mounted IoPackage path below. Do not infer import identities
        // from direct package-import indices if the provider path cannot resolve them.
        var providerCandidates = new[]
        {
            targetPackage,
            targetPackage.TrimStart('/'),
            target.Value.Path
        }.Distinct(StringComparer.OrdinalIgnoreCase);
        CUE4Parse.UE4.Assets.IoPackage? providerLoadedPackage = null;
        foreach (var candidate in providerCandidates)
        {
            try
            {
                if (provider.TryLoadPackage(candidate, out var loadedPackage))
                {
                    Console.WriteLine($"PROVIDER LOAD SUCCESS: candidate={candidate} type={loadedPackage.GetType().FullName} name={loadedPackage.Name} exports={loadedPackage.ExportsLazy.Length}");
                    if (loadedPackage is CUE4Parse.UE4.Assets.IoPackage loadedIoPackage)
                    {
                        providerLoadedPackage = loadedIoPackage;
                        var importedPackagesProperty = loadedIoPackage.GetType().GetProperty("ImportedPackages", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public);
                        var importedPackagesValue = importedPackagesProperty?.GetValue(loadedIoPackage);
                        var importedPackages = importedPackagesValue?.GetType().GetProperty("Value")?.GetValue(importedPackagesValue) as Array;
                        if (importedPackages is null)
                        {
                            Console.WriteLine("PROVIDER IMPORTS: <null>");
                        }
                        else
                        {
                            for (var importedIndex = 0; importedIndex < importedPackages.Length; importedIndex++)
                            {
                                var importedPackage = importedPackages.GetValue(importedIndex);
                                var importedName = importedPackage?.GetType().GetProperty("Name")?.GetValue(importedPackage);
                                Console.WriteLine($"PROVIDER IMPORT[{importedIndex}]: type={importedPackage?.GetType().FullName ?? "<null>"} name={importedName ?? "<null>"}");
                            }
                        }
                    }
                }
                else
                {
                    Console.WriteLine($"PROVIDER LOAD MISS: candidate={candidate}");
                }
            }
            catch (Exception providerException)
            {
                Console.WriteLine($"PROVIDER LOAD FAILURE: candidate={candidate} error={providerException.GetType().Name}: {providerException.Message}");
            }
        }
        var packageIndex = Array.FindIndex(reader.ContainerHeader.PackageIds, p => p.id == target.Key.id);
        Console.WriteLine($"Target container package index={packageIndex}");
        if (packageIndex >= 0 && packageIndex < reader.ContainerHeader.StoreEntries.Length)
            Console.WriteLine($"Target store entry: {reader.ContainerHeader.StoreEntries[packageIndex]}");
        var candidateIds = new[] { target.Key.id };
        foreach (var candidateId in candidateIds)
        for (byte chunkTypeValue = 0; chunkTypeValue <= 8; chunkTypeValue++)
        for (ushort chunkIndex = 0; chunkIndex <= 2; chunkIndex++)
        {
            try
            {
                var chunk = new CUE4Parse.UE4.IO.Objects.FIoChunkId(candidateId, chunkIndex, chunkTypeValue);
                if (!reader.DoesChunkExist(chunk))
                    continue;
                var bytes = reader.Read(chunk);
                Console.WriteLine($"WVF data chunk id={candidateId:X16} type={chunkTypeValue} index={chunkIndex}: bytes={bytes.Length}");
                Console.WriteLine($"WVF bytes: {Convert.ToHexString(bytes.AsSpan(0, Math.Min(bytes.Length, 32)))}");
                try
                {
                    var globalPath = Directory.EnumerateFiles(root, "global.utoc", SearchOption.AllDirectories).FirstOrDefault();
                    if (globalPath is null)
                        throw new FileNotFoundException("global.utoc not found under game Paks root");
                    var globalReader = new CUE4Parse.UE4.IO.IoStoreReader(globalPath, CUE4Parse.UE4.IO.Objects.EIoStoreTocReadOptions.ReadAll, new VersionContainer(EGame.GAME_UE5_5));
                    var globalData = new CUE4Parse.UE4.IO.IoGlobalData(globalReader);
                    Console.WriteLine("IoGlobalData fields: " + string.Join(" | ", globalData.GetType().GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public).Select(f => $"{f.Name}:{f.FieldType.FullName}")));
                    var globalField = globalProperty?.DeclaringType?.GetField("<GlobalData>k__BackingField", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
                    globalField?.SetValue(provider, globalData);
                    Console.WriteLine($"Global data loaded: {globalPath}");
                    var packageArchive = new CUE4Parse.UE4.Readers.FByteArchive(target.Value.Path, bytes, new VersionContainer(EGame.GAME_UE5_5));
                    var exportArchive = new CUE4Parse.UE4.Readers.FByteArchive(target.Value.Path, bytes, new VersionContainer(EGame.GAME_UE5_5));
                    var bulkArchive = new CUE4Parse.UE4.Readers.FByteArchive(target.Value.Path, bytes, new VersionContainer(EGame.GAME_UE5_5));
                    var manualPackage = new CUE4Parse.UE4.Assets.IoPackage(packageArchive, reader.ContainerHeader, exportArchive, bulkArchive, provider);
                    var package = providerLoadedPackage ?? manualPackage;
                    Console.WriteLine($"Analysis package source: {(providerLoadedPackage is null ? "direct reader" : "DefaultFileProvider")}");
                    Console.WriteLine($"IoPackage: name={package.Name} exportType={package.ExportType} canDeserialize={package.CanDeserialize} exports={package.ExportsLazy.Length} names={package.NameMap.Length}");
                    Console.WriteLine("FPackageObjectIndex members: " + string.Join(" | ", typeof(CUE4Parse.UE4.IO.Objects.FPackageObjectIndex).GetMembers().Where(m => m.MemberType is System.Reflection.MemberTypes.Field or System.Reflection.MemberTypes.Property or System.Reflection.MemberTypes.Method).Select(m => m.ToString()).OrderBy(n => n)));
                    Console.WriteLine("FPackageImportReference members: " + string.Join(" | ", typeof(CUE4Parse.UE4.IO.Objects.FPackageImportReference).GetMembers().Where(m => m.MemberType is System.Reflection.MemberTypes.Field or System.Reflection.MemberTypes.Property or System.Reflection.MemberTypes.Method).Select(m => m.ToString()).OrderBy(n => n)));
                    Console.WriteLine("IoPackage imported fields: " + string.Join(" | ", package.GetType().GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public).Where(f => f.Name.Contains("Import", StringComparison.OrdinalIgnoreCase)).Select(f => $"{f.Name}:{f.FieldType.FullName}")));
                    var importedPackagesField = package.GetType().GetField("ImportedPackages", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public);
                    var importedPackagesLazy = importedPackagesField?.GetValue(package);
                    var importedPackages = importedPackagesLazy?.GetType().GetProperty("Value")?.GetValue(importedPackagesLazy) as Array;
                    if (importedPackages is not null)
                    {
                        for (var importedIndex = 0; importedIndex < importedPackages.Length; importedIndex++)
                        {
                            var importedPackage = importedPackages.GetValue(importedIndex);
                            var importedName = importedPackage?.GetType().GetProperty("Name")?.GetValue(importedPackage);
                            Console.WriteLine($"IMPORTED PACKAGE[{importedIndex}]: type={importedPackage?.GetType().FullName ?? "<null>"} name={importedName ?? "<null>"}");
                            if (importedPackage is not null)
                                Console.WriteLine($"  IMPORTED PACKAGE MEMBERS: {string.Join(" | ", importedPackage.GetType().GetProperties(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic).Select(p => p.Name).OrderBy(n => n))}");
                        }
                    }
                    var scriptObjectMapField = globalData.GetType().GetField("ScriptObjectEntriesMap", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public);
                    var scriptObjectMap = scriptObjectMapField?.GetValue(globalData) as System.Collections.IDictionary;
                    var importMapForObjects = package.GetType().GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public)
                        .FirstOrDefault(f => f.Name.Contains("ImportMap", StringComparison.OrdinalIgnoreCase))?.GetValue(package) as Array;
                    if (scriptObjectMap is not null && importMapForObjects is not null)
                    {
                        foreach (var importIndex in new[] { 0, 2 })
                        {
                            if (importIndex < importMapForObjects.Length && importMapForObjects.GetValue(importIndex) is { } importObjectIndex)
                            {
                                object? scriptEntry = null;
                                var wantedTypeAndId = GetNamedMemberValue(importObjectIndex, "TypeAndId")?.ToString();
                                foreach (System.Collections.DictionaryEntry candidate in scriptObjectMap)
                                {
                                    var candidateTypeAndId = candidate.Key is null ? null : GetNamedMemberValue(candidate.Key, "TypeAndId")?.ToString();
                                    if (candidateTypeAndId == wantedTypeAndId)
                                    {
                                        scriptEntry = candidate.Value;
                                        break;
                                    }
                                }
                                Console.WriteLine($"GLOBAL SCRIPT OBJECT import={importIndex}: key={importObjectIndex} value={DescribeKismetValue(scriptEntry, package)}");
                            }
                        }
                    }
                    foreach (var mapField in package.GetType().GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public)
                                 .Where(f => f.Name.Contains("ImportMap", StringComparison.OrdinalIgnoreCase) || f.Name.Contains("ExportMap", StringComparison.OrdinalIgnoreCase)))
                    {
                        var mapValue = mapField.GetValue(package);
                        Console.WriteLine($"PACKAGE MAP {mapField.Name}: {DescribeKismetValue(mapValue, package)}");
                        if (mapValue is Array mapArray)
                        {
                            for (var mapIndex = 0; mapIndex < mapArray.Length; mapIndex++)
                                Console.WriteLine($"  PACKAGE MAP ENTRY {mapField.Name}[{mapIndex}]: {DescribeKismetValue(mapArray.GetValue(mapIndex), package)}");
                        }
                    }
                    foreach (var lazyExport in package.ExportsLazy)
                    {
                        var export = lazyExport.Value;
                        Console.WriteLine($"EXPORT {export.GetType().FullName} name={export.Name}");
                        if (export.Name is "WVF_C" or "ExecuteUbergraph_WVF" or "ExecuteUbergraph_WVF_Actor" or "OnWorldBeginPlay" or "ReceiveBeginPlay" or "On Event Watcher" or "UserConstructionScript")
                        {
                            var exportType = export.GetType();
                            var readableProperties = exportType.GetProperties(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public)
                                .Where(p => p.GetIndexParameters().Length == 0)
                                .Select(p => p.Name)
                                .OrderBy(n => n);
                            Console.WriteLine($"EXPORT MEMBERS {export.Name}: {string.Join(", ", readableProperties)}");
                            if (export.Name is "ExecuteUbergraph_WVF" or "ExecuteUbergraph_WVF_Actor" or "OnWorldBeginPlay" or "ReceiveBeginPlay" or "On Event Watcher" or "UserConstructionScript")
                            {
                                var readableFields = exportType.GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic)
                                    .Where(f => !f.IsStatic)
                                    .Select(f => f.Name)
                                    .OrderBy(n => n);
                                Console.WriteLine($"EXPORT FIELDS {export.Name}: {string.Join(", ", readableFields)}");
                                foreach (var field in exportType.GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic)
                                             .Where(f => !f.IsStatic && (f.Name.Contains("Script", StringComparison.OrdinalIgnoreCase) || f.Name.Contains("Graph", StringComparison.OrdinalIgnoreCase))))
                                {
                                    var fieldValue = field.GetValue(export);
                                    Console.WriteLine($"    FUNCTION FIELD {field.Name}: type={field.FieldType.FullName} value={fieldValue}");
                                    if (field.Name == "ScriptBytecode" && fieldValue is Array bytecode)
                                    {
                                        Console.WriteLine($"    KISMET BYTECODE COUNT {export.Name}: {bytecode.Length}");
                                        for (var bytecodeIndex = 0; bytecodeIndex < bytecode.Length; bytecodeIndex++)
                                        {
                                            var expression = bytecode.GetValue(bytecodeIndex);
                                            if (expression is null)
                                            {
                                                Console.WriteLine($"    KISMET EXPRESSION {export.Name} index={bytecodeIndex}: <null>");
                                                continue;
                                            }

                                            var expressionType = expression.GetType();
                                            var details = expressionType.GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic)
                                                .Where(f => !f.IsStatic)
                                                .Select(f =>
                                                {
                                                    try
                                                    {
                                                        var value = f.GetValue(expression);
                                                        return $"{f.Name}={DescribeKismetValue(value, package)}";
                                                    }
                                                    catch (Exception detailException)
                                                    {
                                                        return $"{f.Name}=<{detailException.GetType().Name}>";
                                                    }
                                                });
                                            Console.WriteLine($"    KISMET EXPRESSION {export.Name} index={bytecodeIndex}: {expressionType.FullName} {string.Join("; ", details)}");
                                        }

                                        var targetEntries = export.Name == "ExecuteUbergraph_WVF"
                                            ? new[] { 15, 1423 }
                                            : export.Name == "ExecuteUbergraph_WVF_Actor" ? new[] { 3903 } : Array.Empty<int>();
                                        foreach (var entry in targetEntries)
                                        {
                                            var start = Math.Max(0, entry - 3);
                                            var end = Math.Min(bytecode.Length - 1, entry + 3);
                                            Console.WriteLine($"    KISMET WINDOW {export.Name} entry={entry} indexes={start}..{end} (entry is a serialized bytecode offset, not an expression-array index)");
                                            for (var index = start; index <= end; index++)
                                            {
                                                var expression = bytecode.GetValue(index);
                                                Console.WriteLine($"      KISMET index={index} type={expression?.GetType().FullName ?? "<null>"} value={expression}");
                                            }
                                        }
                                    }
                                }
                            }
                            try
                            {
                                var serializedProperties = export.Properties;
                                Console.WriteLine($"EXPORT PROPERTY TAGS {export.Name}: count={serializedProperties.Count}");
                                foreach (var property in serializedProperties)
                                {
                                    Console.WriteLine($"  PROPERTY {export.Name}: {property.Name} type={property.GetType().FullName}");
                                    var valueProperty = property.GetType().GetProperty("Value", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic);
                                    if (valueProperty is not null)
                                    {
                                        var value = valueProperty.GetValue(property);
                                        Console.WriteLine($"    VALUE type={value?.GetType().FullName ?? "<null>"} value={value}");
                                    }
                                    else
                                    {
                                        foreach (var field in property.GetType().GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.NonPublic)
                                                     .Where(f => !f.IsStatic).OrderBy(f => f.Name))
                                            Console.WriteLine($"    TAG FIELD {field.Name}: type={field.FieldType.FullName} value={field.GetValue(property)}");
                                    }
                                }
                            }
                            catch (Exception propertyException)
                            {
                                Console.Error.WriteLine($"PROPERTY SAFE FAILURE {export.Name}: {propertyException.GetType().Name}: {propertyException.Message}");
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.Error.WriteLine($"IoPackage SAFE FAILURE: {ex.GetType().Name}: {ex.Message}");
                }
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Chunk id {candidateId:X16} index {chunkIndex}: {ex.GetType().Name}: {ex.Message}");
            }
        }
    }
    var gameFileType = typeof(CUE4Parse.FileProvider.Objects.GameFile);
    Console.WriteLine("GameFile members: " + string.Join(" | ", gameFileType.GetMembers().Where(m => m.MemberType is System.Reflection.MemberTypes.Property or System.Reflection.MemberTypes.Field).Select(m => m.ToString())));
    var packageType = typeof(CUE4Parse.UE4.Assets.IoPackage);
    Console.WriteLine("IoPackage constructors: " + string.Join(" | ", packageType.GetConstructors().Select(c => c.ToString())));
    Console.WriteLine("IoPackage methods: " + string.Join(" | ", packageType.GetMethods().Where(m => m.IsPublic).Select(m => m.ToString()).Distinct().Take(60)));
    var chunkType = typeof(CUE4Parse.UE4.IO.Objects.FIoChunkId);
    Console.WriteLine("FIoChunkId constructors: " + string.Join(" | ", chunkType.GetConstructors().Select(c => c.ToString())));
    Console.WriteLine("FIoChunkId members: " + string.Join(" | ", chunkType.GetMembers().Where(m => m.MemberType is System.Reflection.MemberTypes.Property or System.Reflection.MemberTypes.Field).Select(m => m.ToString())));
    Console.WriteLine("Archive types: " + string.Join(", ", typeof(DefaultFileProvider).Assembly.GetTypes().Where(t => t.Name.Contains("ByteArchive") || t.Name.Contains("MemoryArchive") || t.Name.Contains("AssetArchive")).Select(t => t.FullName)));
    var byteType = typeof(CUE4Parse.UE4.Readers.FByteArchive);
    Console.WriteLine("FByteArchive constructors: " + string.Join(" | ", byteType.GetConstructors().Select(c => c.ToString())));
    Console.WriteLine("Chunk types: " + string.Join(", ", Enum.GetNames(typeof(CUE4Parse.UE4.IO.Objects.EIoChunkType))));
    var packageIdType = typeof(CUE4Parse.UE4.IO.Objects.FPackageId);
    Console.WriteLine("FPackageId members: " + string.Join(" | ", packageIdType.GetMembers().Where(m => m.MemberType is System.Reflection.MemberTypes.Property or System.Reflection.MemberTypes.Field).Select(m => m.ToString())));
    var vfsTypes = typeof(DefaultFileProvider).Assembly.GetTypes().Where(t => t.Name.Contains("VfsEntry", StringComparison.OrdinalIgnoreCase));
    foreach (var t in vfsTypes)
        Console.WriteLine($"Vfs type {t.FullName}: {string.Join(" | ", t.GetConstructors().Select(c => c.ToString()))}");
    var headerType = typeof(CUE4Parse.UE4.IO.Objects.FIoContainerHeader);
    Console.WriteLine("Container header members: " + string.Join(" | ", headerType.GetMembers().Where(m => m.MemberType is System.Reflection.MemberTypes.Property or System.Reflection.MemberTypes.Field).Select(m => m.ToString())));
    var storeType = typeof(CUE4Parse.UE4.IO.Objects.FFilePackageStoreEntry);
    Console.WriteLine("Store entry members: " + string.Join(" | ", storeType.GetMembers().Where(m => m.MemberType is System.Reflection.MemberTypes.Property or System.Reflection.MemberTypes.Field).Select(m => m.ToString())));
    var globalType = typeof(CUE4Parse.UE4.IO.IoGlobalData);
    Console.WriteLine("IoGlobalData constructors: " + string.Join(" | ", globalType.GetConstructors().Select(c => c.ToString())));
    Console.WriteLine("IoGlobalData methods: " + string.Join(" | ", globalType.GetMethods().Where(m => m.IsPublic).Select(m => m.ToString()).Distinct().Take(40)));
    Console.WriteLine("Provider fields: " + string.Join(" | ", typeof(DefaultFileProvider).GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic).Select(f => $"{f.Name}:{f.FieldType.FullName}")));
    Console.WriteLine($"Provider GlobalData property: {globalProperty} setter={globalProperty?.SetMethod}");
    if (globalProperty?.DeclaringType is { } globalDeclaringType)
        Console.WriteLine("GlobalData declaring fields: " + string.Join(" | ", globalDeclaringType.GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic).Select(f => $"{f.Name}:{f.FieldType.FullName}")));
    Console.WriteLine("Reader fields: " + string.Join(" | ", ioType.GetFields(System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic).Select(f => $"{f.Name}:{f.FieldType.FullName}")));
}
foreach (var path in provider.Files.Keys.Take(40))
    Console.WriteLine($"INDEXED {path}");
foreach (var path in provider.Files.Keys.Where(p => p.Contains("Weapon_Viewmodel_FOV", StringComparison.OrdinalIgnoreCase)))
    Console.WriteLine(path);
return 0;
