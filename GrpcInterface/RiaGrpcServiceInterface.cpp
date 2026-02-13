/////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2019-     Equinor ASA
//
//  ResInsight is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  ResInsight is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.
//
//  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
//  for more details.
//
//////////////////////////////////////////////////////////////////////////////////

#include "RiaGrpcServiceInterface.h"
#include "RiaLogging.h"

#include "RimProject.h"

#include "cafPdmAbstractFieldScriptingCapability.h"
#include "cafPdmChildArrayField.h"
#include "cafPdmChildField.h"
#include "cafPdmDataValueField.h"
#include "cafPdmObject.h"
#include "cafPdmObjectScriptingCapability.h"
#include "cafPdmObjectScriptingCapabilityRegister.h"
#include "cafPdmProxyValueField.h"
#include "cafPdmScriptIOMessages.h"
#include "cafPdmXmlFieldHandle.h"

#include <PdmObject.pb.h>
#include <grpcpp/grpcpp.h>

#include <QDebug>
#include <QXmlStreamReader>

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
void RiaGrpcServiceInterface::copyPdmObjectFromCafToRips( const caf::PdmObjectHandle* source, rips::PdmObject* destination )
{
    CAF_ASSERT( source && destination && source->xmlCapability() );

    QString classKeyword = source->xmlCapability()->classKeyword();

    QString scriptClassName;

    // Find first scriptable object in inheritance stack
    {
        auto pdmObject         = dynamic_cast<const caf::PdmObject*>( source );
        auto classKeywordStack = pdmObject->classInheritanceStack();

        // Reverse to get leaf node first
        classKeywordStack.reverse();

        for ( const auto& candidateClassKeyword : classKeywordStack )
        {
            if ( caf::PdmObjectScriptingCapabilityRegister::isScriptable( candidateClassKeyword ) )
            {
                scriptClassName =
                    caf::PdmObjectScriptingCapabilityRegister::scriptClassNameFromClassKeyword( candidateClassKeyword );

                break;
            }
        }

        // Fallback to source object class name
        if ( scriptClassName.isEmpty() ) scriptClassName = classKeyword;
    }

    destination->set_class_keyword( scriptClassName.toStdString() );
    destination->set_address( reinterpret_cast<uint64_t>( source ) );

    bool visible = true;
    if ( source->uiCapability() && source->uiCapability()->objectToggleField() )
    {
        const auto* boolField = dynamic_cast<const caf::PdmField<bool>*>( source->uiCapability()->objectToggleField() );
        if ( boolField )
        {
            visible = boolField->value();
        }
    }
    destination->set_visible( visible );

    std::vector<caf::PdmFieldHandle*> fields = source->fields();

    auto parametersMap = destination->mutable_parameters();
    for ( auto field : fields )
    {
        auto pdmValueField = dynamic_cast<const caf::PdmValueField*>( field );
        if ( !pdmValueField ) continue;

        auto ricfHandle = field->template capability<caf::PdmAbstractFieldScriptingCapability>();
        if ( !ricfHandle ) continue;

        auto pdmProxyField = dynamic_cast<const caf::PdmProxyFieldHandle*>( field );
        if ( pdmProxyField && pdmProxyField->isStreamingField() ) continue;

        std::string          key = ricfHandle->scriptFieldName().toStdString();
        rips::PdmParameterValue paramValue;

        if ( !ricfHandle->enumScriptTexts().empty() )
        {
            // Enum fields: serialize as text representation
            QString     text;
            QTextStream outStream( &text );
            ricfHandle->readFromField( outStream, false );
            paramValue.set_string_value( text.toStdString() );
        }
        else
        {
            QVariant qvar = pdmValueField->toQVariant();
            switch ( qvar.typeId() )
            {
                case QMetaType::Bool:
                    paramValue.set_bool_value( qvar.toBool() );
                    break;
                case QMetaType::Int:
                    paramValue.set_int_value( qvar.toInt() );
                    break;
                case QMetaType::UInt:
                    paramValue.set_uint_value( qvar.toUInt() );
                    break;
                case QMetaType::Float:
                    paramValue.set_float_value( qvar.toFloat() );
                    break;
                case QMetaType::Double:
                    paramValue.set_double_value( qvar.toDouble() );
                    break;
                default:
                {
                    // Fallback: string serialization for compound/unknown types
                    QString     text;
                    QTextStream outStream( &text );
                    ricfHandle->readFromField( outStream, false );
                    paramValue.set_string_value( text.toStdString() );
                    break;
                }
            }
        }

        ( *parametersMap )[key] = paramValue;
    }
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::expected<void, QString> RiaGrpcServiceInterface::copyPdmObjectFromRipsToCaf( const rips::PdmObject* source,
                                                                                  caf::PdmObjectHandle*  destination )
{
    CAF_ASSERT( source && destination && destination->xmlCapability() );

    if ( destination->uiCapability() && destination->uiCapability()->objectToggleField() )
    {
        auto* boolField = dynamic_cast<caf::PdmField<bool>*>( destination->uiCapability()->objectToggleField() );
        if ( boolField )
        {
            QVariant oldValue = boolField->toQVariant();
            boolField->setValue( source->visible() );
            QVariant newValue = boolField->toQVariant();
            destination->uiCapability()->fieldChangedByUi( boolField, oldValue, newValue );
        }
    }

    std::vector<caf::PdmFieldHandle*> fields = destination->fields();

    auto parametersMap = source->parameters();

    // Check for duplicate lowercase keywords. If this happens, the script checking in CAF_PDM_CheckScriptableKeyword in
    // cafPdmFieldScriptingCapability.h is not working as expected
    {
        std::set<QString> uniqueNames;

        for ( const auto& p : parametersMap )
        {
            auto lowerCase = QString::fromStdString( p.first ).toLower();
            if ( uniqueNames.count( lowerCase ) > 0 )
            {
                QString txt = "When receiving an object from Python, multiple key/values for a field keyword was "
                              "detected. This is an error will most likely fail to update the object as intended. "
                              "Keyword name : " +
                              QString::fromStdString( p.first );
                RiaLogging::error( txt );
            }
            uniqueNames.insert( lowerCase );
        }
    }

    bool printContent = false; // Flag to control debug output to debugger. Do not remove this code, as it is useful for
                               // debugging
    if ( printContent )
    {
        for ( const auto& p : parametersMap )
        {
            const auto& paramValue = p.second;
            QString     valueStr;
            switch ( paramValue.value_case() )
            {
                case rips::PdmParameterValue::kBoolValue:
                    valueStr = paramValue.bool_value() ? "true" : "false";
                    break;
                case rips::PdmParameterValue::kIntValue:
                    valueStr = QString::number( paramValue.int_value() );
                    break;
                case rips::PdmParameterValue::kUintValue:
                    valueStr = QString::number( paramValue.uint_value() );
                    break;
                case rips::PdmParameterValue::kFloatValue:
                    valueStr = QString::number( paramValue.float_value() );
                    break;
                case rips::PdmParameterValue::kDoubleValue:
                    valueStr = QString::number( paramValue.double_value() );
                    break;
                case rips::PdmParameterValue::kStringValue:
                    valueStr = QString::fromStdString( paramValue.string_value() );
                    break;
                default:
                    valueStr = "(array or unset)";
                    break;
            }
            qDebug() << QString::fromStdString( p.first ) << " : " << valueStr;
        }
    }

    // Structure to hold pending field changes for two-phase commit
    struct FieldChange
    {
        caf::PdmFieldHandle* field;
        QVariant             oldValue;
        QVariant             newValue;
    };

    // Phase 1: Validate all fields and collect changes (don't commit yet)
    std::vector<FieldChange> pendingChanges;
    QStringList              fieldErrors;

    for ( auto field : fields )
    {
        auto scriptability = field->template capability<caf::PdmAbstractFieldScriptingCapability>();
        if ( scriptability )
        {
            bool isPdmPtrArrayField = ( dynamic_cast<caf::PdmPtrArrayFieldHandle*>( field ) &&
                                        !dynamic_cast<caf::PdmChildArrayFieldHandle*>( field ) );

            if ( !isPdmPtrArrayField && !dynamic_cast<caf::PdmValueField*>( field ) )
            {
                // Update of child objects and child object arrays are not supported
                // Update of PdmPtrArrayField is supported, used by RimcSummaryPlotCollection_newSummaryPlot
                // https://github.com/OPM/ResInsight/issues/7794
                continue;
            }

            QString keyword = scriptability->scriptFieldName();

            // Skip if parameter not provided (don't update field)
            auto it = parametersMap.find( keyword.toStdString() );
            if ( it == parametersMap.end() )
            {
                continue;
            }

            const rips::PdmParameterValue& paramValue = it->second;

            QVariant                 oldValue, newValue;
            caf::PdmScriptIOMessages messages;
            messages.currentCommand  = "Assign value to field " + keyword;
            messages.currentArgument = keyword;

            auto result = assignFieldValue( paramValue, field, &oldValue, &newValue, &messages );

            if ( !result )
            {
                // Validation failed for this field
                fieldErrors.append( QString( "%1: %2" ).arg( keyword, result.error() ) );
            }
            else if ( oldValue != newValue )
            {
                // Value changed and validated - save for commit later
                pendingChanges.push_back( { field, oldValue, newValue } );
            }
        }
    }

    // Object-level validation (only if no field errors so far)
    if ( fieldErrors.isEmpty() )
    {
        std::map<QString, QString> objectErrors = destination->validate();
        for ( const auto& [fieldKeyword, errorMsg] : objectErrors )
        {
            fieldErrors.append( QString( "%1: %2" ).arg( fieldKeyword, errorMsg ) );
        }
    }

    // Phase 2: Commit or rollback based on validation results
    if ( !fieldErrors.isEmpty() )
    {
        // ROLLBACK: Restore all fields to their old values
        for ( const auto& change : pendingChanges )
        {
            auto* valueField = dynamic_cast<caf::PdmValueField*>( change.field );
            if ( valueField )
            {
                valueField->setFromQVariant( change.oldValue );
            }
        }

        // Return aggregated error message
        QString objectClass = destination->xmlCapability() ? destination->xmlCapability()->classKeyword() : "PdmObject";
        return std::unexpected( QString( "Validation failed for %1:\n%2" ).arg( objectClass, fieldErrors.join( "\n" ) ) );
    }

    // SUCCESS: Commit all changes by notifying UI
    for ( const auto& change : pendingChanges )
    {
        destination->uiCapability()->fieldChangedByUi( change.field, change.oldValue, change.newValue );
    }

    return {};
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::expected<void, QString> RiaGrpcServiceInterface::assignFieldValue( const QString&            stringValue,
                                                                        caf::PdmFieldHandle*      field,
                                                                        QVariant*                 oldValue,
                                                                        QVariant*                 newValue,
                                                                        caf::PdmScriptIOMessages* messages )
{
    CAF_ASSERT( oldValue && newValue );

    auto scriptability = field->template capability<caf::PdmAbstractFieldScriptingCapability>();
    if ( !field || !scriptability || !scriptability->isIOWriteable() )
    {
        return {}; // Skip non-writable fields
    }

    auto* valueField = dynamic_cast<caf::PdmValueField*>( field );

    // Step 1: Save old value for potential rollback
    if ( valueField ) *oldValue = valueField->toQVariant();

    // Step 2: Parse and set new value (strict mode for GRPC)
    QTextStream stream( stringValue.toLatin1() );
    scriptability->writeToField( stream, nullptr, messages, false, RimProject::current(), false );

    // Step 3: Check for parsing errors (type mismatches)
    if ( !messages->m_messages.empty() )
    {
        // Parsing failed - collect error messages
        QStringList errors;
        for ( const auto& msg : messages->m_messages )
        {
            if ( msg.first == caf::PdmScriptIOMessages::MESSAGE_ERROR )
            {
                errors.append( msg.second );
            }
        }

        // Rollback to old value
        if ( valueField && oldValue->isValid() )
        {
            valueField->setFromQVariant( *oldValue );
        }

        return errors.isEmpty() ? std::expected<void, QString>{} : std::unexpected( errors.join( "; " ) );
    }

    // Step 4: Get new value after successful parsing
    if ( valueField ) *newValue = valueField->toQVariant();

    // Step 5: Validate the new value (range constraints, etc.)
    QString validationError = field->validate();
    if ( !validationError.isEmpty() )
    {
        // Validation failed - rollback to old value
        if ( valueField && oldValue->isValid() )
        {
            valueField->setFromQVariant( *oldValue );
            *newValue = *oldValue; // Update newValue to reflect rollback
        }
        return std::unexpected( validationError );
    }

    // Success - value is valid and set
    return {};
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
std::expected<void, QString> RiaGrpcServiceInterface::assignFieldValue( const rips::PdmParameterValue& paramValue,
                                                                        caf::PdmFieldHandle*           field,
                                                                        QVariant*                      oldValue,
                                                                        QVariant*                      newValue,
                                                                        caf::PdmScriptIOMessages*      messages )
{
    CAF_ASSERT( oldValue && newValue );

    auto scriptability = field->template capability<caf::PdmAbstractFieldScriptingCapability>();
    if ( !field || !scriptability || !scriptability->isIOWriteable() )
    {
        return {};
    }

    auto* valueField = dynamic_cast<caf::PdmValueField*>( field );

    // Convert typed protobuf value to string representation for writeToField validation.
    // This preserves the existing strict type checking (e.g., rejecting int 1 for bool fields,
    // float 0.99 for int fields) while still benefiting from typed transport.
    QString stringValue;
    switch ( paramValue.value_case() )
    {
        case rips::PdmParameterValue::kBoolValue:
            stringValue = paramValue.bool_value() ? "true" : "false";
            break;
        case rips::PdmParameterValue::kIntValue:
            stringValue = QString::number( paramValue.int_value() );
            break;
        case rips::PdmParameterValue::kUintValue:
            stringValue = QString::number( paramValue.uint_value() );
            break;
        case rips::PdmParameterValue::kFloatValue:
            stringValue = QString::number( paramValue.float_value(), 'g', 15 );
            break;
        case rips::PdmParameterValue::kDoubleValue:
            stringValue = QString::number( paramValue.double_value(), 'g', 15 );
            break;
        case rips::PdmParameterValue::kStringValue:
            stringValue = QString::fromStdString( paramValue.string_value() );
            break;
        case rips::PdmParameterValue::kIntArray:
        {
            QStringList items;
            for ( int v : paramValue.int_array().data() )
                items.append( QString::number( v ) );
            stringValue = "[" + items.join( ", " ) + "]";
            break;
        }
        case rips::PdmParameterValue::kDoubleArray:
        {
            QStringList items;
            for ( double v : paramValue.double_array().data() )
                items.append( QString::number( v, 'g', 15 ) );
            stringValue = "[" + items.join( ", " ) + "]";
            break;
        }
        case rips::PdmParameterValue::kStringArray:
        {
            QStringList items;
            for ( const auto& v : paramValue.string_array().data() )
                items.append( QString::fromStdString( v ) );
            stringValue = "[" + items.join( ", " ) + "]";
            break;
        }
        default:
            return std::unexpected( QString( "Unknown parameter value type" ) );
    }

    messages->currentArgument = stringValue;
    return assignFieldValue( stringValue, field, oldValue, newValue, messages );
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
caf::PdmObjectHandle* RiaGrpcServiceInterface::emplaceChildField( caf::PdmObject* parent,
                                                                  const QString&  fieldKeyword,
                                                                  const QString&  keywordForClassToCreate )
{
    std::vector<caf::PdmFieldHandle*> fields = parent->fields();

    for ( auto field : fields )
    {
        auto pdmChildArrayField = dynamic_cast<caf::PdmChildArrayFieldHandle*>( field );
        auto pdmChildField      = dynamic_cast<caf::PdmChildFieldHandle*>( field );
        if ( pdmChildArrayField )
        {
            bool isMatching = false;
            if ( fieldKeyword.isEmpty() )
            {
                // Use first child array field if no fieldKeyword is specified
                isMatching = true;
            }
            else
            {
                isMatching = ( pdmChildArrayField->keyword() == fieldKeyword );
            }

            if ( isMatching )
            {
                auto objectCreated = emplaceChildArrayField( pdmChildArrayField, keywordForClassToCreate );

                // Notify parent object that a new object has been created
                if ( objectCreated ) parent->onChildAdded( pdmChildArrayField );

                return objectCreated;
            }
        }
        else if ( pdmChildField && pdmChildField->keyword() == fieldKeyword )
        {
            return emplaceChildField( pdmChildField, keywordForClassToCreate );
        }
    }
    return nullptr;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
caf::PdmObjectHandle* RiaGrpcServiceInterface::emplaceChildField( caf::PdmChildFieldHandle* childField,
                                                                  const QString&            keywordForClassToCreate )
{
    QString childClassKeyword;
    if ( keywordForClassToCreate.isEmpty() )
    {
        childClassKeyword = childField->xmlCapability()->dataTypeName();
    }
    else
    {
        childClassKeyword = keywordForClassToCreate;
    }

    auto pdmObjectHandle = caf::PdmDefaultObjectFactory::instance()->create( childClassKeyword );
    CAF_ASSERT( pdmObjectHandle );

    {
        auto childDataTypeName = childField->xmlCapability()->dataTypeName();

        auto isInheritanceValid = pdmObjectHandle->xmlCapability()->inheritsClassWithKeyword( childDataTypeName );
        if ( !isInheritanceValid )
        {
            delete pdmObjectHandle;
            return nullptr;
        }
    }

    childField->setChildObject( pdmObjectHandle );
    return pdmObjectHandle;
}

//--------------------------------------------------------------------------------------------------
///
//--------------------------------------------------------------------------------------------------
caf::PdmObjectHandle* RiaGrpcServiceInterface::emplaceChildArrayField( caf::PdmChildArrayFieldHandle* childArrayField,
                                                                       const QString& keywordForClassToCreate )
{
    QString childClassKeyword;
    if ( keywordForClassToCreate.isEmpty() )
    {
        childClassKeyword = childArrayField->xmlCapability()->dataTypeName();
    }
    else
    {
        childClassKeyword = keywordForClassToCreate;
    }

    auto pdmObjectHandle = caf::PdmDefaultObjectFactory::instance()->create( childClassKeyword );
    if ( !pdmObjectHandle ) return nullptr;

    {
        auto childDataTypeName = childArrayField->xmlCapability()->dataTypeName();

        auto isInheritanceValid = pdmObjectHandle->xmlCapability()->inheritsClassWithKeyword( childDataTypeName );
        if ( !isInheritanceValid )
        {
            delete pdmObjectHandle;
            return nullptr;
        }
    }

    childArrayField->insertAt( -1, pdmObjectHandle );

    return pdmObjectHandle;
}
