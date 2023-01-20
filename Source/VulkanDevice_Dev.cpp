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

#include <ranges>

namespace
{

template< typename To, typename From >
To ClampPix( From v )
{
    return std::clamp( To( v ), To( 96 ), To( 3840 ) );
}

struct WholeWindow
{
    explicit WholeWindow( std::string_view name )
    {
#ifdef IMGUI_HAS_VIEWPORT
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos( viewport->WorkPos );
        ImGui::SetNextWindowSize( viewport->WorkSize );
        ImGui::SetNextWindowViewport( viewport->ID );
#else
        ImGui::SetNextWindowPos( ImVec2( 0.0f, 0.0f ) );
        ImGui::SetNextWindowSize( ImGui::GetIO().DisplaySize );
#endif
        ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );

        if( ImGui::Begin( name.data(),
                          nullptr,
                          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoBackground ) )
        {
            beginSuccess = ImGui::BeginTabBar( "##TabBar", ImGuiTabBarFlags_Reorderable );
        }
    }

    WholeWindow( const WholeWindow& other )                = delete;
    WholeWindow( WholeWindow&& other ) noexcept            = delete;
    WholeWindow& operator=( const WholeWindow& other )     = delete;
    WholeWindow& operator=( WholeWindow&& other ) noexcept = delete;

    explicit operator bool() const { return beginSuccess; }

    ~WholeWindow()
    {
        if( beginSuccess )
        {
            ImGui::EndTabBar();
        }
        ImGui::End();
        ImGui::PopStyleVar( 1 );
    }

private:
    bool beginSuccess{ false };
};

}

void RTGL1::VulkanDevice::Dev_Draw() const
{
    if( !debugWindows || !devmode )
    {
        return;
    }

    if( debugWindows->IsMinimized() )
    {
        return;
    }

    auto w = WholeWindow( "Main window" );
    if( !w )
    {
        return;
    }

    if( ImGui::BeginTabItem( "General" ) )
    {
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.59f, 0.98f, 0.26f, 0.40f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.59f, 0.98f, 0.26f, 1.00f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonActive, ImVec4( 0.53f, 0.98f, 0.06f, 1.00f ) );
        devmode->reloadShaders = ImGui::Button( "Reload shaders", { -1, 96 } );
        ImGui::PopStyleColor( 3 );

        auto& modifiers = devmode->drawInfoOvrd;

        ImGui::Dummy( ImVec2( 0, 4 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 4 ) );

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
            ImGui::Checkbox( "Disable eye adaptation", &modifiers.disableEyeAdaptation );
            ImGui::SliderFloat( "EV100 min", &modifiers.ev100Min, -3, 16, "%.1f" );
            ImGui::SliderFloat( "EV100 max", &modifiers.ev100Max, -3, 16, "%.1f" );
            ImGui::SliderFloat3( "Saturation", modifiers.saturation, -1, 1, "%.1f" );
            ImGui::SliderFloat3( "Crosstalk", modifiers.crosstalk, 0.0f, 1.0f, "%.2f" );
            ImGui::TreePop();
        }
        if( ImGui::TreeNode( "Illumination" ) )
        {
            ImGui::Checkbox( "Anti-firefly", &devmode->antiFirefly );
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
        if( ImGui::TreeNode( "Lightmap" ) )
        {
            ImGui::SliderFloat( "Screen coverage", &modifiers.lightmapScreenCoverage, 0.0f, 1.0f );
            ImGui::TreePop();
        }
        ImGui::EndDisabled();

        ImGui::Dummy( ImVec2( 0, 4 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 4 ) );

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
                { "Normals", DEBUG_SHOW_FLAG_NORMALS },
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

        ImGui::Dummy( ImVec2( 0, 4 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 4 ) );

        devmode->breakOnTexture[ std::size( devmode->breakOnTexture ) - 1 ] = '\0';
        ImGui::TextUnformatted( "Debug break on texture: " );
        ImGui::Checkbox( "Image upload", &devmode->breakOnTextureImage );
        ImGui::Checkbox( "Primitive upload", &devmode->breakOnTexturePrimitive );
        ImGui::InputText( "##Debug break on texture text",
                          devmode->breakOnTexture,
                          std::size( devmode->breakOnTexture ) );

        ImGui::Dummy( ImVec2( 0, 4 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 4 ) );

        ImGui::Checkbox( "Always on top", &devmode->debugWindowOnTop );
        debugWindows->SetAlwaysOnTop( devmode->debugWindowOnTop );

        ImGui::Text( "%.3f ms/frame (%.1f FPS)",
                     1000.0f / ImGui::GetIO().Framerate,
                     ImGui::GetIO().Framerate );
        ImGui::EndTabItem();
    }

    if( ImGui::BeginTabItem( "Primitives" ) )
    {
        ImGui::Checkbox( "Ignore external geometry", &devmode->ignoreExternalGeometry );
        ImGui::Dummy( ImVec2( 0, 4 ) );
        ImGui::Separator();
        ImGui::Dummy( ImVec2( 0, 4 ) );

        using PrimMode = Devmode::DebugPrimMode;

        int*     modePtr = reinterpret_cast< int* >( &devmode->primitivesTableMode );
        PrimMode mode    = devmode->primitivesTableMode;

        ImGui::TextUnformatted( "Record: " );
        ImGui::SameLine();
        ImGui::RadioButton( "None", modePtr, static_cast< int >( PrimMode::None ) );
        ImGui::SameLine();
        ImGui::RadioButton( "Ray-traced", modePtr, static_cast< int >( PrimMode::RayTraced ) );
        ImGui::SameLine();
        ImGui::RadioButton( "Rasterized", modePtr, static_cast< int >( PrimMode::Rasterized ) );
        ImGui::SameLine();
        ImGui::RadioButton( "Non-world", modePtr, static_cast< int >( PrimMode::NonWorld ) );
        ImGui::SameLine();
        ImGui::RadioButton( "Decals", modePtr, static_cast< int >( PrimMode::Decal ) );

        ImGui::TextUnformatted(
            "Red    - if exportable, but not found in GLTF, so uploading as dynamic" );
        ImGui::TextUnformatted( "Green  - if exportable was found in GLTF" );

        if( ImGui::BeginTable( "Primitives table",
                               6,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                   ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY ) )
        {
            {
                ImGui::TableSetupColumn( "Call",
                                         ImGuiTableColumnFlags_NoHeaderWidth |
                                             ImGuiTableColumnFlags_DefaultSort );
                ImGui::TableSetupColumn( "Object ID", ImGuiTableColumnFlags_NoHeaderWidth );
                ImGui::TableSetupColumn( "Mesh name", ImGuiTableColumnFlags_NoHeaderWidth );
                ImGui::TableSetupColumn( "Primitive index", ImGuiTableColumnFlags_NoHeaderWidth );
                ImGui::TableSetupColumn( "Primitive name", ImGuiTableColumnFlags_NoHeaderWidth );
                ImGui::TableSetupColumn( "Texture",
                                         ImGuiTableColumnFlags_NoHeaderWidth |
                                             ImGuiTableColumnFlags_WidthStretch );
                ImGui::TableHeadersRow();
                if( ImGui::IsItemHovered() )
                {
                    ImGui::SetTooltip(
                        "Right-click to open menu\nMiddle-click to copy texture name" );
                }
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
                    if( mode != PrimMode::Decal && mode != PrimMode::NonWorld )
                    {
                        ImGui::Text( "%u", prim.objectId );
                    }

                    ImGui::TableNextColumn();
                    if( mode != PrimMode::Decal && mode != PrimMode::NonWorld )
                    {
                        ImGui::TextUnformatted( prim.meshName.c_str() );
                    }

                    ImGui::TableNextColumn();
                    if( mode != PrimMode::Decal )
                    {
                        ImGui::Text( "%u", prim.primitiveIndex );
                    }

                    ImGui::TableNextColumn();
                    if( mode != PrimMode::Decal )
                    {
                        ImGui::TextUnformatted( prim.primitiveName.c_str() );
                    }

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( prim.textureName.c_str() );
                    if( ImGui::IsMouseReleased( ImGuiMouseButton_Middle ) &&
                        ImGui::IsItemHovered() )
                    {
                        ImGui::SetClipboardText( prim.textureName.c_str() );
                    }
                    else
                    {
                        if( ImGui::BeginPopupContextItem( std::format( "##popup{}", i ).c_str() ) )
                        {
                            if( ImGui::MenuItem( "Copy texture name" ) )
                            {
                                ImGui::SetClipboardText( prim.textureName.c_str() );
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
            }

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }

    if( ImGui::BeginTabItem( "Log" ) )
    {
        ImGui::Checkbox( "Auto-scroll", &devmode->logAutoScroll );
        ImGui::SameLine();
        ImGui::Checkbox( "Compact", &devmode->logCompact );
        ImGui::SameLine();
        if( ImGui::Button( "Clear" ) )
        {
            devmode->logs.clear();
        }
        ImGui::Separator();

        ImGui::CheckboxFlags( "Errors", &devmode->logFlags, RG_MESSAGE_SEVERITY_ERROR );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Warnings", &devmode->logFlags, RG_MESSAGE_SEVERITY_WARNING );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Info", &devmode->logFlags, RG_MESSAGE_SEVERITY_INFO );
        ImGui::SameLine();
        ImGui::CheckboxFlags( "Verbose", &devmode->logFlags, RG_MESSAGE_SEVERITY_VERBOSE );
        ImGui::Separator();

        struct MsgEntry
        {
            uint32_t               count{ 0 };
            RgMessageSeverityFlags severity{ 0 };
            std::string_view       text{};
            uint64_t               hash{ 0 };
        };

        std::deque< MsgEntry > msgs;
        for( auto& [ severity, text, hash ] : devmode->logs | std::ranges::views::reverse )
        {
            bool found = false;

            if( devmode->logCompact )
            {
                for( auto& existing : msgs )
                {
                    if( severity == existing.severity && hash == existing.hash &&
                        text == existing.text )
                    {
                        existing.count++;

                        found = true;
                        break;
                    }
                }
            }

            if( !found )
            {
                msgs.emplace_front( MsgEntry{
                    .count    = 1,
                    .severity = severity,
                    .text     = text,
                    .hash     = std::hash< std::string_view >{}( text ),
                } );
            }
        }


        if( ImGui::BeginChild( "##LogScrollingRegion",
                               ImVec2( 0, 0 ),
                               false,
                               ImGuiWindowFlags_HorizontalScrollbar ) )
        {
            for( const auto& msg : msgs )
            {
                RgMessageSeverityFlags filtered = msg.severity & devmode->logFlags;

                if( filtered == 0 )
                {
                    continue;
                }

                std::optional< ImU32 > color;
                if( filtered & RG_MESSAGE_SEVERITY_ERROR )
                {
                    color = IM_COL32( 255, 0, 0, 255 );
                }
                else if( filtered & RG_MESSAGE_SEVERITY_WARNING )
                {
                    color = IM_COL32( 255, 255, 0, 255 );
                }

                if( color )
                {
                    ImGui::PushStyleColor( ImGuiCol_Text, *color );
                }

                if( msg.count == 1 )
                {
                    ImGui::TextUnformatted( msg.text.data() );
                }
                else
                {
                    ImGui::Text( "[%u] %s", msg.count, msg.text.data() );
                }

                if( color )
                {
                    ImGui::PopStyleColor();
                }
            }

            if( devmode->logAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() )
            {
                ImGui::SetScrollHereY( 1.0f );
            }
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if( ImGui::BeginTabItem( "Import/Export" ) )
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
        ImGui::EndTabItem();
    }

    if( ImGui::BeginTabItem( "Textures" ) )
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

        enum
        {
            ColumnTextureIndex0,
            ColumnTextureIndex1,
            ColumnTextureIndex2,
            ColumnTextureIndex3,
            ColumnMaterialName,
            Column_Count,
        };
        static_assert( std::size( TextureManager::Debug_MaterialInfo{}.textures.indices ) == 4 );

        ImGui::Checkbox( "Record", &devmode->materialsTableEnable );
        ImGui::TextUnformatted( "Blue - if material is non-original (i.e. was loaded from GLTF)" );
        if( ImGui::BeginTable( "Materials table",
                               Column_Count,
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
                                   ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti |
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                   ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY ) )
        {
            auto materialInfos = devmode->materialsTableEnable
                                     ? textureManager->Debug_GetMaterials()
                                     : std::vector< TextureManager::Debug_MaterialInfo >{};
            {
                ImGui::TableSetupColumn( "A", 0, 8 );
                ImGui::TableSetupColumn( "P", 0, 8 );
                ImGui::TableSetupColumn( "N", 0, 8 );
                ImGui::TableSetupColumn( "E", 0, 8 );
                ImGui::TableSetupColumn( "Material name",
                                         ImGuiTableColumnFlags_WidthStretch |
                                             ImGuiTableColumnFlags_DefaultSort,
                                         -1 );
                ImGui::TableHeadersRow();
                if( ImGui::IsItemHovered() )
                {
                    ImGui::SetTooltip(
                        "Right-click to open menu\nMiddle-click to copy texture name" );
                }
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
                                case ColumnTextureIndex0:
                                    ord = ( a.textures.indices[ 0 ] <=> b.textures.indices[ 0 ] );
                                    break;
                                case ColumnTextureIndex1:
                                    ord = ( a.textures.indices[ 1 ] <=> b.textures.indices[ 1 ] );
                                    break;
                                case ColumnTextureIndex2:
                                    ord = ( a.textures.indices[ 2 ] <=> b.textures.indices[ 2 ] );
                                    break;
                                case ColumnTextureIndex3:
                                    ord = ( a.textures.indices[ 3 ] <=> b.textures.indices[ 3 ] );
                                    break;
                                case ColumnMaterialName:
                                    ord = ( a.materialName <=> b.materialName );
                                    break;
                                default: continue;
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
                    ImGui::PushID( i );

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

                    auto writeTexIndex = [ &mat ]( int channel ) {
                        assert( channel >= 0 && channel < std::size( mat.textures.indices ) );
                        if( mat.textures.indices[ channel ] != EMPTY_TEXTURE_INDEX )
                        {
                            ImGui::Text( "%u", mat.textures.indices[ channel ] );
                        }
                    };

                    for( auto col = 0; col < Column_Count; col++ )
                    {
                        ImGui::TableNextColumn();

                        switch( col )
                        {
                            case ColumnTextureIndex0:
                                writeTexIndex( 0 );
                                if( ImGui::TableGetColumnFlags( col ) &
                                    ImGuiTableColumnFlags_IsHovered )
                                {
                                    ImGui::SetTooltip( "Image\n[RGB]Albedo\n[A] "
                                                       "Alpha (0.0 - fully transparent)" );
                                }
                                break;

                            case ColumnTextureIndex1:
                                writeTexIndex( 1 );
                                if( ImGui::TableGetColumnFlags( col ) &
                                    ImGuiTableColumnFlags_IsHovered )
                                {
                                    ImGui::SetTooltip(
                                        "Image\n[R]Occlusion (disabled by default)\n[G] "
                                        "Roughness\n[B] Metallic" );
                                }
                                break;

                            case ColumnTextureIndex2:
                                writeTexIndex( 2 );
                                if( ImGui::TableGetColumnFlags( col ) &
                                    ImGuiTableColumnFlags_IsHovered )
                                {
                                    ImGui::SetTooltip(
                                        "Image\n[R] Normal X offset\n[G] Normal Y offset" );
                                }
                                break;

                            case ColumnTextureIndex3:
                                writeTexIndex( 3 );
                                if( ImGui::TableGetColumnFlags( col ) &
                                    ImGuiTableColumnFlags_IsHovered )
                                {
                                    ImGui::SetTooltip( "Image\n[RGB] Emission color" );
                                }
                                break;

                            case ColumnMaterialName:
                                ImGui::TextUnformatted( mat.materialName.c_str() );

                                if( ImGui::IsMouseReleased( ImGuiMouseButton_Middle ) &&
                                    ImGui::IsItemHovered() )
                                {
                                    ImGui::SetClipboardText( mat.materialName.c_str() );
                                }
                                else
                                {
                                    if( ImGui::BeginPopupContextItem(
                                            std::format( "##popup{}", i ).c_str() ) )
                                    {
                                        if( ImGui::MenuItem( "Copy texture name" ) )
                                        {
                                            ImGui::SetClipboardText( mat.materialName.c_str() );
                                            ImGui::CloseCurrentPopup();
                                        }
                                        ImGui::EndPopup();
                                    }
                                }
                                break;

                            default: break;
                        }
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
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
    RgDrawFrameLightmapParams&         dst_ltmp  = devmode->drawInfoCopy.c_Lightmap;

    auto& modifiers = devmode->drawInfoOvrd;

    if( modifiers.enable )
    {
        // apply modifiers
        dst.vsync            = modifiers.vsync;
        dst.fovYRadians      = Utils::DegToRad( modifiers.fovDeg );

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
            dst_tnmp.disableEyeAdaptation = modifiers.disableEyeAdaptation;
            dst_tnmp.ev100Min             = modifiers.ev100Min;
            dst_tnmp.ev100Max             = modifiers.ev100Max;
            dst_tnmp.saturation           = { RG_ACCESS_VEC3( modifiers.saturation ) };
            dst_tnmp.crosstalk            = { RG_ACCESS_VEC3( modifiers.crosstalk ) };
        }
        {
            dst_ltmp.lightmapScreenCoverage = modifiers.lightmapScreenCoverage;
        }

        return dst;
    }
    else
    {
        // reset modifiers
        modifiers.vsync       = dst.vsync;
        modifiers.fovDeg      = Utils::RadToDeg( dst.fovYRadians );
        devmode->antiFirefly = true;

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
            modifiers.disableEyeAdaptation = dst_tnmp.disableEyeAdaptation;
            modifiers.ev100Min             = dst_tnmp.ev100Min;
            modifiers.ev100Max             = dst_tnmp.ev100Max;
            RG_SET_VEC3_A( modifiers.saturation, dst_tnmp.saturation.data );
            RG_SET_VEC3_A( modifiers.crosstalk, dst_tnmp.crosstalk.data );
        }
        {
            modifiers.lightmapScreenCoverage = dst_ltmp.lightmapScreenCoverage;
        }

        // and return original
        return original;
    }
}

void RTGL1::VulkanDevice::Dev_TryBreak( const char* pTextureName, bool isImageUpload )
{
#ifdef _MSC_VER
    if( !devmode )
    {
        return;
    }

    if( isImageUpload )
    {
        if( !devmode->breakOnTextureImage )
        {
            return;
        }
    }
    else
    {
        if( !devmode->breakOnTexturePrimitive )
        {
            return;
        }
    }

    if( Utils::IsCstrEmpty( devmode->breakOnTexture ) || Utils::IsCstrEmpty( pTextureName ) )
    {
        return;
    }

    devmode->breakOnTexture[ std::size( devmode->breakOnTexture ) - 1 ] = '\0';
    if( std::strcmp( devmode->breakOnTexture, Utils::SafeCstr( pTextureName ) ) == 0 )
    {
        __debugbreak();
        devmode->breakOnTextureImage     = false;
        devmode->breakOnTexturePrimitive = false;
    }
#endif
}
