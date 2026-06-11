using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;
using helengine.editor;

namespace helengine.wii.builder;

/// <summary>
/// Mirrors the shared editor build-graph runtime staging flow for Wii by loading runtime script assemblies,
/// discovering cooked-scene component types, copying the generated native module sources those components need
/// into the shared generated-core root, and emitting matching runtime component deserializers.
/// </summary>
public sealed class WiiRuntimeGeneratedModuleStager {
    /// <summary>
    /// Stages the generated native gameplay module sources and automatic runtime component deserializers needed by the supplied cooked scenes.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root compiled by the Wii native build.</param>
    /// <param name="codeRootPath">Generated native module root that contains one subdirectory per module id.</param>
    /// <param name="cookedSceneAssetPaths">Cooked scene assets whose serialized scripted components drive staging.</param>
    /// <param name="runtimeAssemblyPathsByModuleId">Runtime script assembly paths keyed by module id.</param>
    public void Stage(
        string generatedCoreRootPath,
        string codeRootPath,
        IReadOnlyList<string> cookedSceneAssetPaths,
        IReadOnlyDictionary<string, string> runtimeAssemblyPathsByModuleId) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }
        if (string.IsNullOrWhiteSpace(codeRootPath)) {
            throw new ArgumentException("Code root path must be provided.", nameof(codeRootPath));
        }
        if (cookedSceneAssetPaths == null) {
            throw new ArgumentNullException(nameof(cookedSceneAssetPaths));
        }
        if (runtimeAssemblyPathsByModuleId == null) {
            throw new ArgumentNullException(nameof(runtimeAssemblyPathsByModuleId));
        }

        Directory.CreateDirectory(generatedCoreRootPath);
        ScriptTypeResolver scriptTypeResolver = LoadRuntimeAssemblies(runtimeAssemblyPathsByModuleId);
        IReadOnlyList<Type> componentTypes = DiscoverAutomaticRuntimeComponentTypesFromCookedScenes(cookedSceneAssetPaths, scriptTypeResolver);
        Console.WriteLine($"[WiiBuilder] Discovered runtime component types: {componentTypes.Count}");
        for (int index = 0; index < componentTypes.Count; index++) {
            Console.WriteLine($"[WiiBuilder] ComponentType[{index}]={componentTypes[index].AssemblyQualifiedName}");
        }
        for (int index = 0; index < componentTypes.Count; index++) {
            Type componentType = componentTypes[index];
            string moduleId = componentType.Assembly.GetName().Name ?? string.Empty;
            if (string.IsNullOrWhiteSpace(moduleId)) {
                throw new InvalidOperationException($"Component type '{componentType.FullName}' does not expose a runtime module id.");
            }

            string generatedModuleRootPath = Path.Combine(codeRootPath, moduleId);
            if (!Directory.Exists(generatedModuleRootPath)) {
                throw new DirectoryNotFoundException($"Compiled runtime code module root '{generatedModuleRootPath}' was not found.");
            }

            CopyGeneratedModuleDependencySourcesIntoGeneratedCore(generatedModuleRootPath, generatedCoreRootPath, componentType.Name);
        }

        EmitGeneratedAutomaticRuntimeComponentDeserializers(generatedCoreRootPath, componentTypes);
        PrepareGeneratedCoreRuntimeSupport(generatedCoreRootPath);
        UpdateGeneratedCoreUnityTranslationUnit(generatedCoreRootPath);
    }

    /// <summary>
    /// Applies the Wii-specific generated-core runtime support rewrites required by packaged disc builds regardless of whether any gameplay-generated modules are staged.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root that contains the converted native runtime helpers.</param>
    public void PrepareGeneratedCoreRuntimeSupport(string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        AdaptGeneratedNativePathSupport(generatedCoreRootPath);
    }

    /// <summary>
    /// Loads the supplied runtime script assemblies into one resolver keyed by module id.
    /// </summary>
    /// <param name="runtimeAssemblyPathsByModuleId">Runtime script assembly paths keyed by module id.</param>
    /// <returns>Resolver backed by the loaded runtime assemblies.</returns>
    ScriptTypeResolver LoadRuntimeAssemblies(IReadOnlyDictionary<string, string> runtimeAssemblyPathsByModuleId) {
        ScriptTypeResolver resolver = new ScriptTypeResolver();
        foreach (KeyValuePair<string, string> entry in runtimeAssemblyPathsByModuleId.OrderBy(pair => pair.Key, StringComparer.OrdinalIgnoreCase)) {
            if (string.IsNullOrWhiteSpace(entry.Key)) {
                throw new InvalidOperationException("Runtime assembly module ids must be non-empty.");
            }
            if (string.IsNullOrWhiteSpace(entry.Value)) {
                throw new InvalidOperationException($"Runtime assembly path for module '{entry.Key}' must be provided.");
            }

            string assemblyPath = Path.GetFullPath(entry.Value);
            if (!File.Exists(assemblyPath)) {
                throw new FileNotFoundException($"Runtime assembly '{assemblyPath}' was not found.", assemblyPath);
            }

            Assembly assembly = Assembly.LoadFrom(assemblyPath);
            resolver.Register(entry.Key, assembly);
        }

        return resolver;
    }

    /// <summary>
    /// Discovers the distinct scripted runtime component types referenced by the supplied cooked scenes.
    /// </summary>
    /// <param name="cookedSceneAssetPaths">Cooked scene asset paths whose serialized component records should be inspected.</param>
    /// <param name="scriptTypeResolver">Resolver backed by the loaded gameplay assemblies.</param>
    /// <returns>Distinct scene-referenced runtime component types ordered deterministically.</returns>
    IReadOnlyList<Type> DiscoverAutomaticRuntimeComponentTypesFromCookedScenes(
        IReadOnlyList<string> cookedSceneAssetPaths,
        IScriptTypeResolver scriptTypeResolver) {
        if (cookedSceneAssetPaths == null) {
            throw new ArgumentNullException(nameof(cookedSceneAssetPaths));
        }

        HashSet<Type> componentTypes = new HashSet<Type>();
        for (int index = 0; index < cookedSceneAssetPaths.Count; index++) {
            string cookedSceneAssetPath = cookedSceneAssetPaths[index];
            if (string.IsNullOrWhiteSpace(cookedSceneAssetPath)) {
                continue;
            }
            if (!File.Exists(cookedSceneAssetPath)) {
                throw new FileNotFoundException($"Cooked scene asset '{cookedSceneAssetPath}' was not found.", cookedSceneAssetPath);
            }

            string previousAssetPath = EngineBinaryReadContext.CurrentAssetPath;
            try {
                EngineBinaryReadContext.CurrentAssetPath = cookedSceneAssetPath;
                using FileStream stream = File.OpenRead(cookedSceneAssetPath);
                Asset asset = AssetSerializer.Deserialize(stream);
                if (asset is not SceneAsset sceneAsset) {
                    throw new InvalidOperationException($"Cooked scene '{cookedSceneAssetPath}' did not deserialize into a SceneAsset.");
                }

                CollectAutomaticRuntimeComponentTypes(sceneAsset.RootEntities ?? Array.Empty<SceneEntityAsset>(), scriptTypeResolver, componentTypes);
            } catch (Exception ex) when (ex is not InvalidOperationException || !ex.Message.Contains(cookedSceneAssetPath, StringComparison.Ordinal)) {
                throw new InvalidOperationException($"Cooked scene asset '{cookedSceneAssetPath}' could not be deserialized while discovering automatic runtime components.", ex);
            } finally {
                EngineBinaryReadContext.CurrentAssetPath = previousAssetPath;
            }
        }

        return componentTypes
            .OrderBy(type => type.FullName, StringComparer.Ordinal)
            .ToArray();
    }

    /// <summary>
    /// Recursively collects the scripted component types referenced by one entity tree.
    /// </summary>
    /// <param name="entities">Entity tree whose serialized component records should be inspected.</param>
    /// <param name="scriptTypeResolver">Resolver backed by the loaded gameplay assemblies.</param>
    /// <param name="componentTypes">Set that receives distinct runtime component types.</param>
    void CollectAutomaticRuntimeComponentTypes(
        IReadOnlyList<SceneEntityAsset> entities,
        IScriptTypeResolver scriptTypeResolver,
        HashSet<Type> componentTypes) {
        if (entities == null) {
            throw new ArgumentNullException(nameof(entities));
        }
        if (componentTypes == null) {
            throw new ArgumentNullException(nameof(componentTypes));
        }

        for (int entityIndex = 0; entityIndex < entities.Count; entityIndex++) {
            SceneEntityAsset entity = entities[entityIndex];
            if (entity == null) {
                continue;
            }

            SceneComponentAssetRecord[] components = entity.Components ?? Array.Empty<SceneComponentAssetRecord>();
            for (int componentIndex = 0; componentIndex < components.Length; componentIndex++) {
                SceneComponentAssetRecord componentRecord = components[componentIndex];
                if (componentRecord == null || string.IsNullOrWhiteSpace(componentRecord.ComponentTypeId) || !componentRecord.ComponentTypeId.Contains(',')) {
                    continue;
                }

                Type componentType = ResolveAutomaticRuntimeComponentType(componentRecord.ComponentTypeId, scriptTypeResolver);
                if (!typeof(Component).IsAssignableFrom(componentType)) {
                    throw new InvalidOperationException($"Scene-referenced scripted component type '{componentRecord.ComponentTypeId}' does not derive from Component.");
                }

                componentTypes.Add(componentType);
            }

            CollectAutomaticRuntimeComponentTypes(entity.Children ?? Array.Empty<SceneEntityAsset>(), scriptTypeResolver, componentTypes);
        }
    }

    /// <summary>
    /// Resolves one assembly-qualified scripted component type id to the loaded runtime type.
    /// </summary>
    /// <param name="componentTypeId">Assembly-qualified scripted component type id.</param>
    /// <param name="scriptTypeResolver">Resolver backed by the loaded gameplay assemblies.</param>
    /// <returns>Resolved runtime component type.</returns>
    Type ResolveAutomaticRuntimeComponentType(string componentTypeId, IScriptTypeResolver scriptTypeResolver) {
        if (string.IsNullOrWhiteSpace(componentTypeId)) {
            throw new ArgumentException("Component type id must be provided.", nameof(componentTypeId));
        }

        Type componentType = Type.GetType(componentTypeId, false);
        if (componentType == null && scriptTypeResolver != null) {
            componentType = scriptTypeResolver.Resolve(componentTypeId);
        }
        if (componentType == null) {
            throw new InvalidOperationException($"Scene-referenced scripted component type '{componentTypeId}' could not be resolved for native runtime deserializer generation.");
        }

        return componentType;
    }

    /// <summary>
    /// Emits automatic runtime component deserializers for the supplied component types.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root that should receive the emitted files.</param>
    /// <param name="additionalComponentTypes">Runtime component types that must participate in deserializer emission.</param>
    void EmitGeneratedAutomaticRuntimeComponentDeserializers(string generatedCoreRootPath, IReadOnlyList<Type> additionalComponentTypes) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        Directory.CreateDirectory(generatedCoreRootPath);
        ScriptComponentPlayerDeserializerGenerator generator = new ScriptComponentPlayerDeserializerGenerator();
        ScriptComponentReflectionSchemaBuilder schemaBuilder = new ScriptComponentReflectionSchemaBuilder();
        IReadOnlyList<ScriptComponentReflectionSchema> schemas = DiscoverAutomaticRuntimeComponentSchemas(schemaBuilder, generator, additionalComponentTypes);
        for (int index = 0; index < schemas.Count; index++) {
            ScriptComponentReflectionSchema schema = schemas[index];
            string className = generator.BuildNativeDeserializerClassName(schema);
            File.WriteAllText(
                Path.Combine(generatedCoreRootPath, className + ".hpp"),
                generator.GenerateNativeDeserializerHeader(schema));
            File.WriteAllText(
                Path.Combine(generatedCoreRootPath, className + ".cpp"),
                generator.GenerateNativeDeserializerSource(schema));
        }

        WriteGeneratedRuntimeComponentDeserializerRegistrationFromGeneratedCore(generatedCoreRootPath);
    }

    /// <summary>
    /// Rebuilds the generated runtime component deserializer registration unit from the deserializer headers currently present in generated-core.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root that contains the generated runtime deserializer headers.</param>
    void WriteGeneratedRuntimeComponentDeserializerRegistrationFromGeneratedCore(string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        Directory.CreateDirectory(generatedCoreRootPath);
        string[] generatedDeserializerClassNames = Directory
            .GetFiles(generatedCoreRootPath, "GeneratedRuntime*Deserializer.hpp", SearchOption.TopDirectoryOnly)
            .Select(Path.GetFileNameWithoutExtension)
            .Where(className => !string.IsNullOrWhiteSpace(className))
            .Where(className => !string.Equals(className, "GeneratedRuntimeComponentDeserializerRegistration", StringComparison.OrdinalIgnoreCase))
            .OrderBy(className => className, StringComparer.Ordinal)
            .ToArray();

        File.WriteAllText(
            Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.hpp"),
            BuildGeneratedRuntimeComponentDeserializerRegistrationHeader());
        File.WriteAllText(
            Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.cpp"),
            BuildGeneratedRuntimeComponentDeserializerRegistrationSource(generatedDeserializerClassNames));
    }

    /// <summary>
    /// Rewrites generated native path helpers so Wii <c>dvd:/</c> virtual roots bypass host filesystem normalization.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root that contains the converted native path helpers.</param>
    void AdaptGeneratedNativePathSupport(string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        AdaptGeneratedPathSource(Path.Combine(generatedCoreRootPath, "system", "io", "path.cpp"));
    }

    /// <summary>
    /// Rewrites the generated native path helper source so Wii packaged-disc virtual roots remain rooted at <c>dvd:/</c>.
    /// </summary>
    /// <param name="filePath">Generated native path helper source path.</param>
    void AdaptGeneratedPathSource(string filePath) {
        if (string.IsNullOrWhiteSpace(filePath)) {
            throw new ArgumentException("File path must be provided.", nameof(filePath));
        }
        if (!File.Exists(filePath)) {
            return;
        }

        string contents = File.ReadAllText(filePath);
        if (contents.Contains("IsWiiDevicePath", StringComparison.Ordinal)) {
            return;
        }

        string newline = contents.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        string updatedContents = contents;
        string namespaceNeedle = "#endif" + newline + newline + "std::string Path::Combine(const std::string& left, const std::string& right) {";
        string wiiNamespace = """
#if HE_CPP_PLATFORM_WII
namespace {
    bool IsWiiDevicePath(const std::string& path) {
        return path.rfind("dvd:/", 0) == 0;
    }
}
#endif

""";
        updatedContents = updatedContents.Replace(
            namespaceNeedle,
            "#endif" + newline + newline + wiiNamespace.Replace("\n", newline, StringComparison.Ordinal) + "std::string Path::Combine(const std::string& left, const std::string& right) {",
            StringComparison.Ordinal);

        string combineGuard = """
#if HE_CPP_PLATFORM_WII
    if (IsWiiDevicePath(left)) {
        if (right.empty()) {
            return left;
        }

        if (right[0] == '/') {
            return left + right.substr(1);
        }

        return left + right;
    }
    if (IsWiiDevicePath(right)) {
        return right;
    }
#endif
""";
        updatedContents = updatedContents.Replace(
            "std::string Path::Combine(const std::string& left, const std::string& right) {" + newline,
            "std::string Path::Combine(const std::string& left, const std::string& right) {" + newline + combineGuard.Replace("\n", newline, StringComparison.Ordinal) + newline,
            StringComparison.Ordinal);

        string directoryGuard = """
#if HE_CPP_PLATFORM_WII
    if (IsWiiDevicePath(path)) {
        std::size_t separatorIndex = path.find_last_of("/\\");
        if (separatorIndex == std::string::npos) {
            return std::string();
        }

        if (separatorIndex <= 4) {
            return std::string("dvd:/");
        }

        return path.substr(0, separatorIndex);
    }
#endif
""";
        updatedContents = updatedContents.Replace(
            "std::string Path::GetDirectoryName(const std::string& path) {" + newline + "    if (path.empty()) {" + newline + "        return std::string();" + newline + "    }" + newline + newline,
            "std::string Path::GetDirectoryName(const std::string& path) {" + newline + "    if (path.empty()) {" + newline + "        return std::string();" + newline + "    }" + newline + newline + directoryGuard.Replace("\n", newline, StringComparison.Ordinal) + newline,
            StringComparison.Ordinal);

        string fileNameGuard = """
#if HE_CPP_PLATFORM_WII
    if (IsWiiDevicePath(path)) {
        std::size_t separatorIndex = path.find_last_of("/\\");
        return separatorIndex == std::string::npos ? path : path.substr(separatorIndex + 1);
    }
#endif
""";
        updatedContents = updatedContents.Replace(
            "std::string Path::GetFileName(const std::string& path) {" + newline + "    if (path.empty()) {" + newline + "        return std::string();" + newline + "    }" + newline + newline,
            "std::string Path::GetFileName(const std::string& path) {" + newline + "    if (path.empty()) {" + newline + "        return std::string();" + newline + "    }" + newline + newline + fileNameGuard.Replace("\n", newline, StringComparison.Ordinal) + newline,
            StringComparison.Ordinal);

        string fullPathGuard = """
#if HE_CPP_PLATFORM_WII
    if (IsWiiDevicePath(path)) {
        return path;
    }
#endif
""";
        updatedContents = updatedContents.Replace(
            "std::string Path::GetFullPath(const std::string& path) {" + newline,
            "std::string Path::GetFullPath(const std::string& path) {" + newline + fullPathGuard.Replace("\n", newline, StringComparison.Ordinal) + newline,
            StringComparison.Ordinal);

        string rootedGuard = """
#if HE_CPP_PLATFORM_WII
    if (IsWiiDevicePath(path)) {
        return true;
    }
#endif
""";
        updatedContents = updatedContents.Replace(
            "bool Path::IsPathRooted(const std::string& path) {" + newline + "    if (path.empty()) {" + newline + "        return false;" + newline + "    }" + newline + newline,
            "bool Path::IsPathRooted(const std::string& path) {" + newline + "    if (path.empty()) {" + newline + "        return false;" + newline + "    }" + newline + newline + rootedGuard.Replace("\n", newline, StringComparison.Ordinal) + newline,
            StringComparison.Ordinal);

        if (!string.Equals(contents, updatedContents, StringComparison.Ordinal)) {
            File.WriteAllText(filePath, updatedContents);
        }
    }

    /// <summary>
    /// Discovers the generated deserializer schemas supported by the supplied runtime component types.
    /// </summary>
    /// <param name="schemaBuilder">Reflected schema builder used for component discovery.</param>
    /// <param name="generator">Native deserializer generator used to validate supported schemas.</param>
    /// <param name="additionalComponentTypes">Runtime component types that must participate in deserializer emission.</param>
    /// <returns>Deterministically ordered deserializer schemas.</returns>
    IReadOnlyList<ScriptComponentReflectionSchema> DiscoverAutomaticRuntimeComponentSchemas(
        ScriptComponentReflectionSchemaBuilder schemaBuilder,
        ScriptComponentPlayerDeserializerGenerator generator,
        IReadOnlyList<Type> additionalComponentTypes) {
        if (schemaBuilder == null) {
            throw new ArgumentNullException(nameof(schemaBuilder));
        }
        if (generator == null) {
            throw new ArgumentNullException(nameof(generator));
        }

        HashSet<Type> requiredAdditionalComponentTypes = new HashSet<Type>();
        if (additionalComponentTypes != null) {
            for (int index = 0; index < additionalComponentTypes.Count; index++) {
                Type additionalComponentType = additionalComponentTypes[index];
                if (additionalComponentType == null) {
                    continue;
                }
                if (!IsEligibleAutomaticRuntimeComponentType(additionalComponentType)) {
                    throw new InvalidOperationException($"Scene-referenced scripted component type '{additionalComponentType.FullName}' is not eligible for automatic native runtime deserializer generation.");
                }

                requiredAdditionalComponentTypes.Add(additionalComponentType);
            }
        }

        HashSet<Type> componentTypes = new HashSet<Type>(
            typeof(Component).Assembly
                .GetTypes()
                .Where(IsEligibleAutomaticRuntimeComponentType)
                .OrderBy(type => type.FullName, StringComparer.Ordinal));
        foreach (Type additionalComponentType in requiredAdditionalComponentTypes) {
            componentTypes.Add(additionalComponentType);
        }

        List<Type> orderedComponentTypes = componentTypes
            .OrderBy(type => type.FullName, StringComparer.Ordinal)
            .ToList();
        List<ScriptComponentReflectionSchema> schemas = new List<ScriptComponentReflectionSchema>(orderedComponentTypes.Count);
        for (int index = 0; index < orderedComponentTypes.Count; index++) {
            Type componentType = orderedComponentTypes[index];
            ScriptComponentReflectionSchema schema = schemaBuilder.Build(componentType);
            if (generator.CanGenerateNativeDeserializer(schema)) {
                schemas.Add(schema);
                continue;
            }
            if (requiredAdditionalComponentTypes.Contains(componentType)) {
                throw new InvalidOperationException($"Native runtime deserializer generation does not support scene-referenced scripted component type '{componentType.FullName}'.");
            }
        }

        return schemas;
    }

    /// <summary>
    /// Returns whether one component type can participate in generated native runtime deserializer emission.
    /// </summary>
    /// <param name="componentType">Component type to inspect.</param>
    /// <returns>True when the component type is eligible for generated deserializer emission.</returns>
    bool IsEligibleAutomaticRuntimeComponentType(Type componentType) {
        if (componentType == null) {
            return false;
        }
        if (componentType == typeof(Component) || componentType == typeof(UpdateComponent)) {
            return false;
        }
        if (!typeof(Component).IsAssignableFrom(componentType)) {
            return false;
        }
        if (!componentType.IsClass || componentType.IsAbstract || componentType.ContainsGenericParameters) {
            return false;
        }
        if (string.IsNullOrWhiteSpace(componentType.FullName)) {
            return false;
        }
        if (HasExplicitRuntimeComponentDeserializer(componentType)) {
            return false;
        }

        return componentType.GetConstructor(Type.EmptyTypes) != null;
    }

    /// <summary>
    /// Returns whether one component type is already covered by one hand-authored runtime deserializer.
    /// </summary>
    /// <param name="componentType">Component type to inspect.</param>
    /// <returns>True when the component already has one explicit runtime deserializer.</returns>
    bool HasExplicitRuntimeComponentDeserializer(Type componentType) {
        if (componentType == null) {
            return false;
        }

        return componentType == typeof(MeshComponent)
            || componentType == typeof(CameraComponent)
            || componentType == typeof(SceneMapComponent);
    }

    /// <summary>
    /// Builds the generated native registration header used to install all emitted automatic runtime component deserializers.
    /// </summary>
    /// <returns>Generated native registration header text.</returns>
    string BuildGeneratedRuntimeComponentDeserializerRegistrationHeader() {
        return "#pragma once" + Environment.NewLine
            + "#ifdef DrawText" + Environment.NewLine
            + "#undef DrawText" + Environment.NewLine
            + "#endif" + Environment.NewLine
            + "class RuntimeComponentRegistry;" + Environment.NewLine + Environment.NewLine
            + "void RegisterGeneratedRuntimeComponentDeserializers(::RuntimeComponentRegistry* registry);" + Environment.NewLine;
    }

    /// <summary>
    /// Builds the generated native registration source used to install all emitted automatic runtime component deserializers.
    /// </summary>
    /// <param name="generatedDeserializerClassNames">Generated deserializer class names that should be registered.</param>
    /// <returns>Generated native registration source text.</returns>
    string BuildGeneratedRuntimeComponentDeserializerRegistrationSource(
        IReadOnlyList<string> generatedDeserializerClassNames) {
        if (generatedDeserializerClassNames == null) {
            throw new ArgumentNullException(nameof(generatedDeserializerClassNames));
        }

        StringBuilder builder = new StringBuilder();
        builder.AppendLine("#ifdef DrawText");
        builder.AppendLine("#undef DrawText");
        builder.AppendLine("#endif");
        builder.AppendLine("#include \"GeneratedRuntimeComponentDeserializerRegistration.hpp\"");
        builder.AppendLine("#include \"RuntimeComponentRegistry.hpp\"");
        builder.AppendLine("#include \"runtime/native_exceptions.hpp\"");
        for (int index = 0; index < generatedDeserializerClassNames.Count; index++) {
            builder.AppendLine($"#include \"{generatedDeserializerClassNames[index]}.hpp\"");
        }

        builder.AppendLine();
        builder.AppendLine("void RegisterGeneratedRuntimeComponentDeserializers(::RuntimeComponentRegistry* registry)");
        builder.AppendLine("{");
        builder.AppendLine("    if (registry == nullptr)");
        builder.AppendLine("    {");
        builder.AppendLine("throw new ArgumentNullException(\"registry\");");
        builder.AppendLine("    }");
        for (int index = 0; index < generatedDeserializerClassNames.Count; index++) {
            builder.AppendLine($"registry->Register(new ::{generatedDeserializerClassNames[index]}());");
        }

        builder.AppendLine("}");
        return builder.ToString();
    }

    /// <summary>
    /// Copies the scene-referenced generated module source files into the shared generated-core root.
    /// </summary>
    /// <param name="generatedModuleRootPath">Generated runtime module output root produced by the authored-code phase.</param>
    /// <param name="generatedCoreRootPath">Shared generated-core root consumed by the Wii native build.</param>
    /// <param name="componentTypeName">Generated gameplay component type name whose authored same-module dependencies should be staged.</param>
    void CopyGeneratedModuleDependencySourcesIntoGeneratedCore(string generatedModuleRootPath, string generatedCoreRootPath, string componentTypeName) {
        if (string.IsNullOrWhiteSpace(generatedModuleRootPath)) {
            throw new ArgumentException("Generated module root path must be provided.", nameof(generatedModuleRootPath));
        }
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }
        if (string.IsNullOrWhiteSpace(componentTypeName)) {
            throw new ArgumentException("Component type name must be provided.", nameof(componentTypeName));
        }

        Queue<string> pendingRelativePaths = new Queue<string>();
        HashSet<string> visitedRelativePaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        pendingRelativePaths.Enqueue(componentTypeName + ".hpp");
        pendingRelativePaths.Enqueue(componentTypeName + ".cpp");
        while (pendingRelativePaths.Count > 0) {
            string relativePath = pendingRelativePaths.Dequeue();
            if (!visitedRelativePaths.Add(relativePath)) {
                continue;
            }

            string sourcePath = Path.Combine(generatedModuleRootPath, relativePath);
            if (!File.Exists(sourcePath)) {
                continue;
            }

            CopyGeneratedDependencySourceIfNeeded(generatedCoreRootPath, componentTypeName, relativePath, sourcePath);
            EnqueueLocalGeneratedModuleIncludes(generatedModuleRootPath, relativePath, sourcePath, pendingRelativePaths, visitedRelativePaths);
        }
    }

    /// <summary>
    /// Copies one authored generated native dependency source file into the shared generated-core root when it is not already supplied there.
    /// </summary>
    /// <param name="generatedCoreRootPath">Shared generated-core root consumed by the Wii native build.</param>
    /// <param name="componentTypeName">Gameplay component type name whose authored support files are being staged.</param>
    /// <param name="relativePath">Module-relative native source path being staged.</param>
    /// <param name="sourcePath">Absolute authored generated source path.</param>
    void CopyGeneratedDependencySourceIfNeeded(string generatedCoreRootPath, string componentTypeName, string relativePath, string sourcePath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }
        if (string.IsNullOrWhiteSpace(componentTypeName)) {
            throw new ArgumentException("Component type name must be provided.", nameof(componentTypeName));
        }
        if (string.IsNullOrWhiteSpace(relativePath)) {
            throw new ArgumentException("Relative path must be provided.", nameof(relativePath));
        }
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }

        string destinationPath = Path.Combine(generatedCoreRootPath, relativePath);
        string destinationDirectoryPath = Path.GetDirectoryName(destinationPath)
            ?? throw new InvalidOperationException($"Generated-core destination directory could not be resolved for '{relativePath}'.");
        Directory.CreateDirectory(destinationDirectoryPath);
        bool isComponentRootSource = string.Equals(relativePath, componentTypeName + ".hpp", StringComparison.OrdinalIgnoreCase)
            || string.Equals(relativePath, componentTypeName + ".cpp", StringComparison.OrdinalIgnoreCase);
        if (File.Exists(destinationPath)) {
            string existingContents = File.ReadAllText(destinationPath);
            string newContents = File.ReadAllText(sourcePath);
            if (string.Equals(existingContents, newContents, StringComparison.Ordinal)) {
                return;
            }
            if (isComponentRootSource) {
                return;
            }

            throw new InvalidOperationException($"Generated runtime module source '{relativePath}' conflicts with an existing generated-core source file.");
        }

        File.Copy(sourcePath, destinationPath, true);
    }

    /// <summary>
    /// Enqueues local authored generated native includes referenced by one module source file so same-module support types become available to generated-core compilation.
    /// </summary>
    /// <param name="generatedModuleRootPath">Generated runtime module output root produced by the authored-code phase.</param>
    /// <param name="relativePath">Module-relative path of the source file whose includes should be scanned.</param>
    /// <param name="sourcePath">Absolute authored generated source path.</param>
    /// <param name="pendingRelativePaths">Pending module-relative paths that still need to be processed.</param>
    /// <param name="visitedRelativePaths">Already processed module-relative paths used to avoid cycles.</param>
    void EnqueueLocalGeneratedModuleIncludes(
        string generatedModuleRootPath,
        string relativePath,
        string sourcePath,
        Queue<string> pendingRelativePaths,
        ISet<string> visitedRelativePaths) {
        if (string.IsNullOrWhiteSpace(generatedModuleRootPath)) {
            throw new ArgumentException("Generated module root path must be provided.", nameof(generatedModuleRootPath));
        }
        if (string.IsNullOrWhiteSpace(relativePath)) {
            throw new ArgumentException("Relative path must be provided.", nameof(relativePath));
        }
        if (string.IsNullOrWhiteSpace(sourcePath)) {
            throw new ArgumentException("Source path must be provided.", nameof(sourcePath));
        }
        if (pendingRelativePaths == null) {
            throw new ArgumentNullException(nameof(pendingRelativePaths));
        }
        if (visitedRelativePaths == null) {
            throw new ArgumentNullException(nameof(visitedRelativePaths));
        }

        string sourceText = File.ReadAllText(sourcePath);
        MatchCollection includeMatches = Regex.Matches(
            sourceText,
            "#include\\s+\"([^\"]+)\"",
            RegexOptions.CultureInvariant);
        for (int index = 0; index < includeMatches.Count; index++) {
            string includeRelativePath = ResolveLocalGeneratedModuleIncludeRelativePath(generatedModuleRootPath, relativePath, includeMatches[index].Groups[1].Value);
            if (string.IsNullOrWhiteSpace(includeRelativePath)) {
                continue;
            }
            if (!visitedRelativePaths.Contains(includeRelativePath)) {
                pendingRelativePaths.Enqueue(includeRelativePath);
            }

            EnqueueGeneratedModuleCompanionSourceIfPresent(generatedModuleRootPath, includeRelativePath, pendingRelativePaths, visitedRelativePaths);
        }
    }

    /// <summary>
    /// Resolves one local generated-module include path to a normalized module-relative path when the include targets a same-module source file.
    /// </summary>
    /// <param name="generatedModuleRootPath">Generated runtime module output root produced by the authored-code phase.</param>
    /// <param name="sourceRelativePath">Module-relative source file path that declared the include.</param>
    /// <param name="includePath">Raw include path declared in the source file.</param>
    /// <returns>Normalized module-relative include path, or an empty string when the include does not target a same-module file.</returns>
    string ResolveLocalGeneratedModuleIncludeRelativePath(string generatedModuleRootPath, string sourceRelativePath, string includePath) {
        if (string.IsNullOrWhiteSpace(generatedModuleRootPath)) {
            throw new ArgumentException("Generated module root path must be provided.", nameof(generatedModuleRootPath));
        }
        if (string.IsNullOrWhiteSpace(sourceRelativePath)) {
            throw new ArgumentException("Source relative path must be provided.", nameof(sourceRelativePath));
        }
        if (string.IsNullOrWhiteSpace(includePath)) {
            return string.Empty;
        }

        string sourceDirectoryPath = Path.GetDirectoryName(sourceRelativePath) ?? string.Empty;
        string candidateRelativePath = string.IsNullOrWhiteSpace(sourceDirectoryPath)
            ? includePath
            : Path.Combine(sourceDirectoryPath, includePath);
        string candidateFullPath = Path.GetFullPath(Path.Combine(generatedModuleRootPath, candidateRelativePath));
        string normalizedRootPath = Path.GetFullPath(generatedModuleRootPath);
        if (!candidateFullPath.StartsWith(normalizedRootPath, StringComparison.OrdinalIgnoreCase)) {
            return string.Empty;
        }
        if (!File.Exists(candidateFullPath)) {
            return string.Empty;
        }

        return Path.GetRelativePath(generatedModuleRootPath, candidateFullPath);
    }

    /// <summary>
    /// Enqueues the companion implementation file for one local generated-module header when that implementation exists beside the include target.
    /// </summary>
    /// <param name="generatedModuleRootPath">Generated runtime module output root produced by the authored-code phase.</param>
    /// <param name="includeRelativePath">Module-relative local include path that was already resolved.</param>
    /// <param name="pendingRelativePaths">Pending module-relative paths that still need to be processed.</param>
    /// <param name="visitedRelativePaths">Already processed module-relative paths used to avoid cycles.</param>
    void EnqueueGeneratedModuleCompanionSourceIfPresent(
        string generatedModuleRootPath,
        string includeRelativePath,
        Queue<string> pendingRelativePaths,
        ISet<string> visitedRelativePaths) {
        if (string.IsNullOrWhiteSpace(generatedModuleRootPath)) {
            throw new ArgumentException("Generated module root path must be provided.", nameof(generatedModuleRootPath));
        }
        if (string.IsNullOrWhiteSpace(includeRelativePath)) {
            return;
        }
        if (pendingRelativePaths == null) {
            throw new ArgumentNullException(nameof(pendingRelativePaths));
        }
        if (visitedRelativePaths == null) {
            throw new ArgumentNullException(nameof(visitedRelativePaths));
        }
        if (!string.Equals(Path.GetExtension(includeRelativePath), ".hpp", StringComparison.OrdinalIgnoreCase)) {
            return;
        }

        string companionRelativePath = Path.ChangeExtension(includeRelativePath, ".cpp");
        string companionSourcePath = Path.Combine(generatedModuleRootPath, companionRelativePath);
        if (!File.Exists(companionSourcePath)) {
            return;
        }
        if (visitedRelativePaths.Contains(companionRelativePath)) {
            return;
        }

        pendingRelativePaths.Enqueue(companionRelativePath);
    }

    /// <summary>
    /// Appends any newly staged generated-core source files to the unity translation unit so the native build compiles them.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated core root whose unity translation unit should be refreshed.</param>
    void UpdateGeneratedCoreUnityTranslationUnit(string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }
        if (!Directory.Exists(generatedCoreRootPath)) {
            return;
        }

        string unitySourcePath = Path.Combine(generatedCoreRootPath, "helengine_core_unity.cpp");
        string[] excludedSourceRelativePaths = [
            "runtime/runtime_startup_manifest.cpp",
            "runtime/runtime_scene_catalog_manifest.cpp",
            "runtime/runtime_code_module_manifest.cpp"
        ];
        List<string> sourceFiles = new List<string>();
        string[] discoveredFiles = Directory.GetFiles(generatedCoreRootPath, "*.cpp", SearchOption.AllDirectories);
        for (int index = 0; index < discoveredFiles.Length; index++) {
            string sourceFilePath = discoveredFiles[index];
            string fileName = Path.GetFileName(sourceFilePath);
            if (string.Equals(fileName, "helengine_core_amalgamated.cpp", StringComparison.OrdinalIgnoreCase)
                || string.Equals(fileName, "helengine_core_unity.cpp", StringComparison.OrdinalIgnoreCase)) {
                continue;
            }

            string relativePath = Path.GetRelativePath(generatedCoreRootPath, sourceFilePath).Replace('\\', '/');
            if (excludedSourceRelativePaths.Contains(relativePath, StringComparer.OrdinalIgnoreCase)) {
                continue;
            }

            sourceFiles.Add(relativePath);
        }

        sourceFiles.Sort(StringComparer.OrdinalIgnoreCase);
        HashSet<string> existingIncludePaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        List<string> preservedLines = new List<string>();
        if (File.Exists(unitySourcePath)) {
            string[] existingLines = File.ReadAllLines(unitySourcePath);
            for (int index = 0; index < existingLines.Length; index++) {
                string line = existingLines[index];
                preservedLines.Add(line);
                string includePath = TryExtractUnityIncludePath(line);
                if (!string.IsNullOrWhiteSpace(includePath)) {
                    existingIncludePaths.Add(includePath);
                }
            }
        } else {
            preservedLines.Add("// Generated compile-validation unity translation unit.");
            preservedLines.Add(string.Empty);
        }

        for (int index = 0; index < sourceFiles.Count; index++) {
            string relativePath = sourceFiles[index];
            if (existingIncludePaths.Contains(relativePath)) {
                continue;
            }

            preservedLines.Add($"#include \"{relativePath}\"");
        }

        File.WriteAllLines(unitySourcePath, preservedLines);
    }

    /// <summary>
    /// Extracts one include path from a unity translation-unit include line.
    /// </summary>
    /// <param name="line">Candidate unity translation-unit line.</param>
    /// <returns>Included relative path, or an empty string when the line is not a quoted include.</returns>
    string TryExtractUnityIncludePath(string line) {
        if (string.IsNullOrWhiteSpace(line)) {
            return string.Empty;
        }

        Match match = Regex.Match(line, "^#include\\s+\"([^\"]+)\"$", RegexOptions.CultureInvariant);
        if (!match.Success) {
            return string.Empty;
        }

        return match.Groups[1].Value;
    }
}
