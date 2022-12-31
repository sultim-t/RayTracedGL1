// Copyright (c) 2022 Sultim Tsyrendashiev
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "VulkanDevice.h"

#include "Generated/ShaderCommonC.h"

#include <imgui.h>

namespace
{

template< typename To, typename From >
To ClampPix( From v )
{
    return std::clamp( To( v ), To( 96 ), To( 3840 ) );
}

}

void RTGL1::VulkanDevice::Dev_Draw() const
{
    if( !debugWindows || !devmode )
    {
        return;
    }

    if( ImGui::Begin( "General" ), nullptr, ImGuiWindowFlags_HorizontalScrollbar )
    {
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.59f, 0.98f, 0.26f, 0.40f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.59f, 0.98f, 0.26f, 1.00f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.53f, 0.98f, 0.06f, 1.00f ) );
        devmode->reloadShaders = ImGui::Button( "Reload shaders", { -1, 96 } );
        ImGui::PopStyleColor( 3 );

        auto& modifiers = devmode->drawInfoOvrd;

        ImGui::Separator();
        ImGui::Checkbox( "Override", &modifiers.enable );
        ImGui::BeginDisabled( !modifiers.enable );
        if( ImGui::TreeNode( "Present" ) )
        {
            ImGui::Checkbox( "Vsync", &modifiers.vsync );
            ImGui::SliderFloat( "Vertical FOV", &modifiers.fovDeg, 10, 120, "%.0f degrees" );

            static_assert(
                std::is_same_v< int, std::underlying_type_t< RgRenderUpscaleTechnique > > );
            static_assert(
                std::is_same_v< int, std::underlying_type_t< RgRenderSharpenTechnique > > );
            static_assert(
                std::is_same_v< int, std::underlying_type_t< RgRenderResolutionMode > > );

            bool dlssOk = IsUpscaleTechniqueAvailable( RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS );
            {
                ImGui::RadioButton( "Linear##Upscale",
                                    reinterpret_cast< int* >( &modifiers.upscaleTechnique ),
                                    RG_RENDER_UPSCALE_TECHNIQUE_LINEAR );
                ImGui::SameLine();
                ImGui::RadioButton( "Nearest##Upscale",
                                    reinterpret_cast< int* >( &modifiers.upscaleTechnique ),
                                    RG_RENDER_UPSCALE_TECHNIQUE_NEAREST );
                ImGui::SameLine();
                ImGui::RadioButton( "FSR 2.1##Upscale",
                                    reinterpret_cast< int* >( &modifiers.upscaleTechnique ),
                                    RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2 );
                ImGui::SameLine();
                ImGui::BeginDisabled( !dlssOk );
                ImGui::RadioButton( "DLSS 2##Upscale",
                                    reinterpret_cast< int* >( &modifiers.upscaleTechnique ),
                                    RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS );
                ImGui::EndDisabled();
            }
            {
                ImGui::RadioButton( "None##Sharp",
                                    reinterpret_cast< int* >( &modifiers.sharpenTechnique ),
                                    RG_RENDER_SHARPEN_TECHNIQUE_NONE );
                ImGui::SameLine();
                ImGui::RadioButton( "Naive sharpening##Sharp",
                                    reinterpret_cast< int* >( &modifiers.sharpenTechnique ),
                                    RG_RENDER_SHARPEN_TECHNIQUE_NAIVE );
                ImGui::SameLine();
                ImGui::RadioButton( "AMD CAS sharpening##Sharp",
                                    reinterpret_cast< int* >( &modifiers.sharpenTechnique ),
                                    RG_RENDER_SHARPEN_TECHNIQUE_AMD_CAS );
            }

            bool forceCustom =
                modifiers.upscaleTechnique != RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2 &&
                modifiers.upscaleTechnique != RG_RENDER_UPSCALE_TECHNIQUE_NVIDIA_DLSS;
            if( forceCustom )
            {
                modifiers.resolutionMode = RG_RENDER_RESOLUTION_MODE_CUSTOM;
            }

            {
                ImGui::RadioButton( "Custom##Resolution",
                                    reinterpret_cast< int* >( &modifiers.resolutionMode ),
                                    RG_RENDER_RESOLUTION_MODE_CUSTOM );
                ImGui::SameLine();
                ImGui::BeginDisabled( forceCustom );
                ImGui::RadioButton( "Ultra Performance##Resolution",
                                    reinterpret_cast< int* >( &modifiers.resolutionMode ),
                                    RG_RENDER_RESOLUTION_MODE_ULTRA_PERFORMANCE );
                ImGui::SameLine();
                ImGui::RadioButton( "Performance##Resolution",
                                    reinterpret_cast< int* >( &modifiers.resolutionMode ),
                                    RG_RENDER_RESOLUTION_MODE_PERFORMANCE );
                ImGui::SameLine();
                ImGui::RadioButton( "Balanced##Resolution",
                                    reinterpret_cast< int* >( &modifiers.resolutionMode ),
                                    RG_RENDER_RESOLUTION_MODE_BALANCED );
                ImGui::SameLine();
                ImGui::RadioButton( "Quality##Resolution",
                                    reinterpret_cast< int* >( &modifiers.resolutionMode ),
                                    RG_RENDER_RESOLUTION_MODE_QUALITY );
                ImGui::SameLine();
                ImGui::BeginDisabled( modifiers.upscaleTechnique ==
                                      RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2 );
                ImGui::RadioButton( "Ultra Quality##Resolution",
                                    reinterpret_cast< int* >( &modifiers.resolutionMode ),
                                    RG_RENDER_RESOLUTION_MODE_ULTRA_QUALITY );
                ImGui::EndDisabled();
                ImGui::EndDisabled();
            }
            {
                ImGui::BeginDisabled(
                    !( modifiers.resolutionMode == RG_RENDER_RESOLUTION_MODE_CUSTOM ) );

                ImGui::SliderInt2( "Custom render size", modifiers.customRenderSize, 96, 3840 );

                ImGui::EndDisabled();
            }

            ImGui::TreePop();
        }
        if( ImGui::TreeNode( "Tonemapping" ) )
        {
            ImGui::SliderFloat( "EV100 min", &modifiers.ev100Min, -3, 16, "%.1f" );
            ImGui::SliderFloat( "EV100 max", &modifiers.ev100Max, -3, 16, "%.1f" );
            ImGui::SliderFloat3( "Saturation", modifiers.saturation, -1, 1, "%.1f" );
            ImGui::SliderFloat3( "Crosstalk", modifiers.crosstalk, 0.0f, 1.0f, "%.2f" );
            ImGui::TreePop();
        }
        if( ImGui::TreeNode( "Illumination" ) )
        {
            ImGui::Checkbox( "Anti-firefly", &modifiers.antiFirefly );
            ImGui::SliderInt( "Shadow rays max depth",
                              &modifiers.maxBounceShadows,
                              0,
                              2,
                              "%d",
                              ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput );
            ImGui::Checkbox( "Second bounce for indirect",
                             &modifiers.enableSecondBounceForIndirect );
            ImGui::SliderFloat( "Sensitivity to change: Diffuse Direct",
                                &modifiers.directDiffuseSensitivityToChange,
                                0.0f,
                                1.0f,
                                "%.2f" );
            ImGui::SliderFloat( "Sensitivity to change: Diffuse Indirect",
                                &modifiers.indirectDiffuseSensitivityToChange,
                                0.0f,
                                1.0f,
                                "%.2f" );
            ImGui::SliderFloat( "Sensitivity to change: Specular",
                                &modifiers.specularSensitivityToChange,
                                0.0f,
                                1.0f,
                                "%.2f" );
            ImGui::TreePop();
        }
        ImGui::EndDisabled();

        if( ImGui::TreeNode( "Debug show" ) )
        {
            std::pair< const char*, uint32_t > fs[] = {
                { "Unfiltered diffuse direct", DEBUG_SHOW_FLAG_UNFILTERED_DIFFUSE },
                { "Unfiltered diffuse indirect", DEBUG_SHOW_FLAG_UNFILTERED_INDIRECT },
                { "Unfiltered specular", DEBUG_SHOW_FLAG_UNFILTERED_SPECULAR },
                { "Diffuse direct", DEBUG_SHOW_FLAG_ONLY_DIRECT_DIFFUSE },
                { "Diffuse indirect", DEBUG_SHOW_FLAG_ONLY_INDIRECT_DIFFUSE },
                { "Specular", DEBUG_SHOW_FLAG_ONLY_SPECULAR },
                { "Albedo white", DEBUG_SHOW_FLAG_ALBEDO_WHITE },
                { "Motion vectors", DEBUG_SHOW_FLAG_MOTION_VECTORS },
                { "Gradients", DEBUG_SHOW_FLAG_GRADIENTS },
                { "Light grid", DEBUG_SHOW_FLAG_LIGHT_GRID },
            };
            for( const auto [ name, f ] : fs )
            {
                ImGui::CheckboxFlags( name, &devmode->debugShowFlags, f );
            }
            ImGui::TreePop();
        }

        ImGui::Checkbox( "Always on top", &devmode->debugWindowOnTop );
        debugWindows->SetAlwaysOnTop( devmode->debugWindowOnTop );

        ImGui::Text( "%.3f ms/frame (%.1f FPS)",
                     1000.0f / ImGui::GetIO().Framerate,
                     ImGui::GetIO().Framerate );
    }
    ImGui::End();

    if( ImGui::Begin( "Primitives", nullptr, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        ImGui::RadioButton( "Disable", &devmode->primitivesTableEnable, 0 );
        ImGui::SameLine();
        ImGui::RadioButton( "Record rasterized", &devmode->primitivesTableEnable, 1 );
        ImGui::SameLine();
        ImGui::RadioButton( "Record ray-traced", &devmode->primitivesTableEnable, 2 );

        ImGui::TextUnformatted(
            "Red    - if exportable, but not found in GLTF, so uploading as dynamic" );
        ImGui::TextUnformatted( "Green  - if exportable was found in GLTF" );

        if( ImGui::BeginTable( "Primitives table",
                               6,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders ) )
        {
            {
                ImGui::TableSetupColumn( "Call" );
                ImGui::TableSetupColumn( "Object ID" );
                ImGui::TableSetupColumn( "Mesh name" );
                ImGui::TableSetupColumn( "Primitive index" );
                ImGui::TableSetupColumn( "Primitive name" );
                ImGui::TableSetupColumn( "Texture" );
                ImGui::TableHeadersRow();
            }

            if( ImGuiTableSortSpecs* sortspecs = ImGui::TableGetSortSpecs() )
            {
                sortspecs->SpecsDirty = true;

                std::ranges::sort(
                    devmode->primitivesTable,
                    [ sortspecs ]( const Devmode::DebugPrim& a,
                                   const Devmode::DebugPrim& b ) -> bool {
                        for( int n = 0; n < sortspecs->SpecsCount; n++ )
                        {
                            const ImGuiTableColumnSortSpecs* srt = &sortspecs->Specs[ n ];

                            std::strong_ordering ord{ 0 };
                            switch( srt->ColumnIndex )
                            {
                                case 0: ord = ( a.callIndex <=> b.callIndex ); break;
                                case 1: ord = ( a.objectId <=> b.objectId ); break;
                                case 2: ord = ( a.meshName <=> b.meshName ); break;
                                case 3: ord = ( a.primitiveIndex <=> b.primitiveIndex ); break;
                                case 4: ord = ( a.primitiveName <=> b.primitiveName ); break;
                                case 5: ord = ( a.textureName <=> b.textureName ); break;
                                default: assert( 0 ); return false;
                            }

                            if( std::is_gt( ord ) )
                            {
                                return srt->SortDirection != ImGuiSortDirection_Ascending;
                            }

                            if( std::is_lt( ord ) )
                            {
                                return srt->SortDirection == ImGuiSortDirection_Ascending;
                            }
                        }

                        return a.callIndex < b.callIndex;
                    } );
            }

            ImGuiListClipper clipper;
            clipper.Begin( int( devmode->primitivesTable.size() ) );
            while( clipper.Step() )
            {
                for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
                {
                    const auto& prim = devmode->primitivesTable[ i ];
                    ImGui::TableNextRow();

                    if( prim.result == UploadResult::ExportableStatic )
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0,
                                                IM_COL32( 0, 128, 0, 64 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1,
                                                IM_COL32( 0, 128, 0, 128 ) );
                    }
                    else if( prim.result == UploadResult::ExportableDynamic )
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0,
                                                IM_COL32( 128, 0, 0, 64 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1,
                                                IM_COL32( 128, 0, 0, 128 ) );
                    }
                    else
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, IM_COL32( 0, 0, 0, 1 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, IM_COL32( 0, 0, 0, 1 ) );
                    }


                    ImGui::TableNextColumn();
                    if( prim.result != UploadResult::Fail )
                    {
                        ImGui::Text( "%u", prim.callIndex );
                    }
                    else
                    {
                        ImGui::TextUnformatted( "fail" );
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text( "%u", prim.objectId );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( prim.meshName.c_str() );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%u", prim.primitiveIndex );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( prim.primitiveName.c_str() );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( prim.textureName.c_str() );
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();

    if( ImGui::Begin( "Non-world Primitives", nullptr, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        ImGui::Checkbox( "Record", &devmode->nonworldTableEnable );

        if( ImGui::BeginTable( "Non-world Primitives table",
                               2,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders ) )
        {
            {
                ImGui::TableSetupColumn( "Call" );
                ImGui::TableSetupColumn( "Texture" );
                ImGui::TableHeadersRow();
            }

            for( const auto& prim : devmode->nonworldTable )
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text( "%u", prim.callIndex );
                ImGui::TableNextColumn();
                ImGui::TextUnformatted( prim.textureName.c_str() );
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();

    if( ImGui::Begin( "Log", nullptr, ImGuiWindowFlags_HorizontalScrollbar ) )
    {
        ImGui::CheckboxFlags( "Errors", &devmode->logFlags, RG_MESSAGE_SEVERITY_ERROR );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Warnings", &devmode->logFlags, RG_MESSAGE_SEVERITY_WARNING );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Info", &devmode->logFlags, RG_MESSAGE_SEVERITY_INFO );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Verbose", &devmode->logFlags, RG_MESSAGE_SEVERITY_VERBOSE );

        if( ImGui::Button( "Clear" ) )
        {
            devmode->logs.clear();
        }

        for( const auto& [ severity, msg ] : devmode->logs )
        {
            RgMessageSeverityFlags filtered = severity & devmode->logFlags;

            ImU32 color;
            if( filtered & RG_MESSAGE_SEVERITY_ERROR )
            {
                color = IM_COL32( 255, 0, 0, 255 );
            }
            else if( filtered & RG_MESSAGE_SEVERITY_WARNING )
            {
                color = IM_COL32( 255, 255, 0, 255 );
            }
            else if( filtered & RG_MESSAGE_SEVERITY_INFO )
            {
                color = IM_COL32( 255, 255, 255, 255 );
            }
            else if( filtered & RG_MESSAGE_SEVERITY_VERBOSE )
            {
                color = IM_COL32( 255, 255, 255, 255 );
            }
            else
            {
                assert( filtered == 0 );
                continue;
            }
            ImGui::PushStyleColor( ImGuiCol_Text, color );
            ImGui::TextUnformatted( msg.c_str() );
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();


    if( ImGui::Begin( "Import/Export" ) )
    {
        auto& dev = sceneImportExport->dev;
        if( !dev.exportName.enable )
        {
            dev.exportName.SetDefaults( *sceneImportExport );
        }
        if( !dev.importName.enable )
        {
            dev.importName.SetDefaults( *sceneImportExport );
        }
        if( !dev.worldTransform.enable )
        {
            dev.worldTransform.SetDefaults( *sceneImportExport );
        }

        {
            ImGui::Text( "Resource folder: %s",
                         std::filesystem::absolute( ovrdFolder ).string().c_str() );
        }
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );
        {
            if( ImGui::Button( "Reimport GLTF", { -1, 80 } ) )
            {
                sceneImportExport->RequestReimport();
            }

            ImGui::Text( "Import path: %s",
                         sceneImportExport->MakeGltfPath( sceneImportExport->GetImportMapName() )
                             .string()
                             .c_str() );
            ImGui::BeginDisabled( !dev.importName.enable );
            {
                ImGui::InputText(
                    "Import map name", dev.importName.value, std::size( dev.importName.value ) );
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Checkbox( "Custom##import", &dev.importName.enable );
        }
        ImGui::Dummy( ImVec2( 0, 16 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );
        {
            ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.98f, 0.59f, 0.26f, 0.40f ) );
            ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.98f, 0.59f, 0.26f, 1.00f ) );
            ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.98f, 0.53f, 0.06f, 1.00f ) );
            if( ImGui::Button( "Export frame geometry", { -1, 80 } ) )
            {
                sceneImportExport->RequestExport();
            }
            ImGui::PopStyleColor( 3 );

            ImGui::Text( "Export path: %s",
                         sceneImportExport->MakeGltfPath( sceneImportExport->GetExportMapName() )
                             .string()
                             .c_str() );
            ImGui::BeginDisabled( !dev.exportName.enable );
            {
                ImGui::InputText(
                    "Export map name", dev.exportName.value, std::size( dev.exportName.value ) );
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Checkbox( "Custom##export", &dev.exportName.enable );
        }
        ImGui::Dummy( ImVec2( 0, 16 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );
        {
            ImGui::Checkbox( "Custom import/export world space", &dev.worldTransform.enable );
            ImGui::BeginDisabled( !dev.worldTransform.enable );
            {
                ImGui::SliderFloat3( "World Up vector", dev.worldTransform.up.data, -1.0f, 1.0f );
                ImGui::SliderFloat3(
                    "World Forward vector", dev.worldTransform.forward.data, -1.0f, 1.0f );
                ImGui::InputFloat(
                    std::format( "1 unit = {} meters", dev.worldTransform.scale ).c_str(),
                    &dev.worldTransform.scale );
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::End();


    if( ImGui::Begin( "Textures" ), nullptr, ImGuiWindowFlags_HorizontalScrollbar )
    {
        if( ImGui::Button( "Export original textures", { -1, 80 } ) )
        {
            textureManager->ExportOriginalMaterialTextures( ovrdFolder /
                                                            TEXTURES_FOLDER_ORIGINALS );
        }
        ImGui::Text( "Export path: %s",
                     ( ovrdFolder / TEXTURES_FOLDER_ORIGINALS ).string().c_str() );
        ImGui::Dummy( ImVec2( 0, 16 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 16 ) );

        ImGui::Checkbox( "Record", &devmode->materialsTableEnable );
        ImGui::TextUnformatted( "Blue - if material is non-original (i.e. was loaded from GLTF)" );
        if( ImGui::BeginTable( "Materials table",
                               1,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders ) )
        {
            auto materialInfos = devmode->materialsTableEnable
                                     ? textureManager->Debug_GetMaterials()
                                     : std::vector< TextureManager::Debug_MaterialInfo >{};
            {
                ImGui::TableSetupColumn( "Material name" );
                ImGui::TableHeadersRow();
            }

            if( ImGuiTableSortSpecs* sortspecs = ImGui::TableGetSortSpecs() )
            {
                sortspecs->SpecsDirty = true;

                std::ranges::sort(
                    materialInfos,
                    [ sortspecs ]( const TextureManager::Debug_MaterialInfo& a,
                                   const TextureManager::Debug_MaterialInfo& b ) -> bool {
                        for( int n = 0; n < sortspecs->SpecsCount; n++ )
                        {
                            const ImGuiTableColumnSortSpecs* srt = &sortspecs->Specs[ n ];

                            std::strong_ordering ord{ 0 };
                            switch( srt->ColumnIndex )
                            {
                                case 0: ord = ( a.materialName <=> b.materialName ); break;
                                default: assert( 0 ); return false;
                            }

                            if( std::is_gt( ord ) )
                            {
                                return srt->SortDirection != ImGuiSortDirection_Ascending;
                            }

                            if( std::is_lt( ord ) )
                            {
                                return srt->SortDirection == ImGuiSortDirection_Ascending;
                            }
                        }

                        return a.materialName < b.materialName;
                    } );
            }

            ImGuiListClipper clipper;
            clipper.Begin( int( materialInfos.size() ) );
            while( clipper.Step() )
            {
                for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ )
                {
                    const auto& mat = materialInfos[ i ];
                    ImGui::TableNextRow();

                    if( mat.isOriginal )
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0,
                                                IM_COL32( 0, 0, 128, 64 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1,
                                                IM_COL32( 0, 0, 128, 128 ) );
                    }
                    else
                    {
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg0, IM_COL32( 0, 0, 0, 1 ) );
                        ImGui::TableSetBgColor( ImGuiTableBgTarget_RowBg1, IM_COL32( 0, 0, 0, 1 ) );
                    }

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( mat.materialName.c_str() );
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

const RgDrawFrameInfo& RTGL1::VulkanDevice::Dev_Override( const RgDrawFrameInfo& original ) const
{
    if( !debugWindows || !devmode )
    {
        return original;
    }

    // in devmode, deep copy original info to modify it
    {
        devmode->drawInfoCopy.c                  = original;
        devmode->drawInfoCopy.c_RenderResolution = AccessParams( original.pRenderResolutionParams );
        devmode->drawInfoCopy.c_Illumination     = AccessParams( original.pIlluminationParams );
        devmode->drawInfoCopy.c_Volumetric       = AccessParams( original.pVolumetricParams );
        devmode->drawInfoCopy.c_Tonemapping      = AccessParams( original.pTonemappingParams );
        devmode->drawInfoCopy.c_Bloom            = AccessParams( original.pBloomParams );
        devmode->drawInfoCopy.c_ReflectRefract   = AccessParams( original.pReflectRefractParams );
        devmode->drawInfoCopy.c_Sky              = AccessParams( original.pSkyParams );
        devmode->drawInfoCopy.c_Textures         = AccessParams( original.pTexturesParams );
        devmode->drawInfoCopy.c_Lightmap         = AccessParams( original.pLightmapParams );

        // dynamic defaults
        {
            devmode->drawInfoCopy.c_RenderResolution.customRenderSize = {
                renderResolution.UpscaledWidth(), renderResolution.UpscaledHeight()
            };
        }

        // relink
        devmode->drawInfoCopy.c.pRenderResolutionParams = &devmode->drawInfoCopy.c_RenderResolution;
        devmode->drawInfoCopy.c.pIlluminationParams     = &devmode->drawInfoCopy.c_Illumination;
        devmode->drawInfoCopy.c.pVolumetricParams       = &devmode->drawInfoCopy.c_Volumetric;
        devmode->drawInfoCopy.c.pTonemappingParams      = &devmode->drawInfoCopy.c_Tonemapping;
        devmode->drawInfoCopy.c.pBloomParams            = &devmode->drawInfoCopy.c_Bloom;
        devmode->drawInfoCopy.c.pReflectRefractParams   = &devmode->drawInfoCopy.c_ReflectRefract;
        devmode->drawInfoCopy.c.pSkyParams              = &devmode->drawInfoCopy.c_Sky;
        devmode->drawInfoCopy.c.pTexturesParams         = &devmode->drawInfoCopy.c_Textures;
        devmode->drawInfoCopy.c.pLightmapParams         = &devmode->drawInfoCopy.c_Lightmap;
    }

    RgDrawFrameInfo&                   dst       = devmode->drawInfoCopy.c;
    RgDrawFrameRenderResolutionParams& dst_resol = devmode->drawInfoCopy.c_RenderResolution;
    RgDrawFrameIlluminationParams&     dst_illum = devmode->drawInfoCopy.c_Illumination;
    RgDrawFrameTonemappingParams&      dst_tnmp  = devmode->drawInfoCopy.c_Tonemapping;

    auto& modifiers = devmode->drawInfoOvrd;

    if( modifiers.enable )
    {
        // apply modifiers
        dst.vsync            = modifiers.vsync;
        dst.fovYRadians      = Utils::DegToRad( modifiers.fovDeg );
        dst.forceAntiFirefly = modifiers.antiFirefly;

        {
            dst_resol.upscaleTechnique = modifiers.upscaleTechnique;
            dst_resol.sharpenTechnique = modifiers.sharpenTechnique;
            dst_resol.resolutionMode   = modifiers.resolutionMode;
            dst_resol.customRenderSize = { ClampPix< uint32_t >( modifiers.customRenderSize[ 0 ] ),
                                           ClampPix< uint32_t >(
                                               modifiers.customRenderSize[ 1 ] ) };
            modifiers.pixelizedForPtr  = { ClampPix< uint32_t >( modifiers.pixelized[ 0 ] ),
                                           ClampPix< uint32_t >( modifiers.pixelized[ 1 ] ) };
            dst_resol.pPixelizedRenderSize =
                modifiers.pixelizedEnable ? &modifiers.pixelizedForPtr : nullptr;
        }
        {
            dst_illum.maxBounceShadows                 = modifiers.maxBounceShadows;
            dst_illum.enableSecondBounceForIndirect    = modifiers.enableSecondBounceForIndirect;
            dst_illum.directDiffuseSensitivityToChange = modifiers.directDiffuseSensitivityToChange;
            dst_illum.indirectDiffuseSensitivityToChange =
                modifiers.indirectDiffuseSensitivityToChange;
            dst_illum.specularSensitivityToChange = modifiers.specularSensitivityToChange;
        }
        {
            dst_tnmp.ev100Min   = modifiers.ev100Min;
            dst_tnmp.ev100Max   = modifiers.ev100Max;
            dst_tnmp.saturation = { RG_ACCESS_VEC3( modifiers.saturation ) };
            dst_tnmp.crosstalk  = { RG_ACCESS_VEC3( modifiers.crosstalk ) };
        }

        return dst;
    }
    else
    {
        // reset modifiers
        modifiers.vsync       = dst.vsync;
        modifiers.fovDeg      = Utils::RadToDeg( dst.fovYRadians );
        modifiers.antiFirefly = dst.forceAntiFirefly;

        {
            modifiers.upscaleTechnique = dst_resol.upscaleTechnique;
            modifiers.sharpenTechnique = dst_resol.sharpenTechnique;
            modifiers.resolutionMode   = dst_resol.resolutionMode;

            modifiers.customRenderSize[ 0 ] = ClampPix< int >( dst_resol.customRenderSize.width );
            modifiers.customRenderSize[ 1 ] = ClampPix< int >( dst_resol.customRenderSize.height );

            modifiers.pixelizedEnable = dst_resol.pPixelizedRenderSize != nullptr;

            modifiers.pixelized[ 0 ] =
                dst_resol.pPixelizedRenderSize
                    ? ClampPix< int >( dst_resol.pPixelizedRenderSize->width )
                    : 0;
            modifiers.pixelized[ 1 ] =
                dst_resol.pPixelizedRenderSize
                    ? ClampPix< int >( dst_resol.pPixelizedRenderSize->height )
                    : 0;
        }
        {
            modifiers.maxBounceShadows                 = int( dst_illum.maxBounceShadows );
            modifiers.enableSecondBounceForIndirect    = dst_illum.enableSecondBounceForIndirect;
            modifiers.directDiffuseSensitivityToChange = dst_illum.directDiffuseSensitivityToChange;
            modifiers.indirectDiffuseSensitivityToChange =
                dst_illum.indirectDiffuseSensitivityToChange;
            modifiers.specularSensitivityToChange = dst_illum.specularSensitivityToChange;
        }
        {
            modifiers.ev100Min = dst_tnmp.ev100Min;
            modifiers.ev100Max = dst_tnmp.ev100Max;
            RG_SET_VEC3_A( modifiers.saturation, dst_tnmp.saturation.data );
            RG_SET_VEC3_A( modifiers.crosstalk, dst_tnmp.crosstalk.data );
        }

        // and return original
        return original;
    }
}
