// Example 1C script that uses the UapkiNative component
// Works both in thick client and in managed application modules

Процедура ИнициализироватьКомпонент()
    ПодключитьВнешнююКомпоненту("uapki", "UapkiNative", ТипВнешнейКомпоненты.Native);
    Если Не ЗначениеЗаполнено(Компонента) Тогда
        Компонента = Новый("AddIn.UapkiNative");
        ПараметрыJSON =
            '{"certCache":{"path":"/opt/uapki/certs","trustedCerts":[]},"crlCache":{"path":"/opt/uapki/crls"}}';
        Если Не Компонента.Initialize(ПараметрыJSON) Тогда
            ВызватьИсключение Компонента.LastError;
        КонецЕсли;
    КонецЕсли;
КонецПроцедуры

Функция ПодписатьФайл(ПутьКФайлу, ПутьКПодписи)
    Параметры =
        '{"signParams":{"signatureFormat":"CAdES-BES","signAlgo":"1.2.804.2.1.1.1.1.3.1.1","includeCert":true}}';
    Результат = Компонента.SignFile(ПутьКФайлу, ПутьКПодписи, Параметры);
    Возврат JSONПрочитать(Результат);
КонецФункции

Функция ПроверитьПодпись(ПутьКФайлу, ПутьКПодписи)
    Опции = '{"validationType":"CHAIN"}';
    Ответ = Компонента.VerifyFileSignature(ПутьКФайлу, ПутьКПодписи, Опции);
    Возврат JSONПрочитать(Ответ);
КонецФункции

