#pragma once

const char update_html[] PROGMEM = R"(
<form method="POST" action="/fw" enctype="multipart/form-data">
    <input type="hidden" name="size" id="size">
    <input id="file" type="file" accept=".bin" name="update">
    <input id="submit" type="submit">
</form>

<script>
    var fileEl = document.getElementById("file");
    var submit = document.getElementById("submit");
    submit.onclick = function () {
        if (!fileEl.files.length) {
            alert("You must pick a firmware file");
            return false;
        }
        var file = fileEl.files[0];

        document.getElementById("size").setAttribute("value", file.size);
        return true;
    }
</script>
)";