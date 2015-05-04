%include('header.tpl', title="", refresh=10)

%include('menu.tpl', current='assoc')

%if len(active) == 0:
<p>No active Songcast Receivers found</p>
%elif  len(others) == 0:
<p>No inactive Songcast Receivers found</p>
%else:

<form action="/assoc" method="POST">
<table>
    <caption>Choose Master receiver</caption>
  <tr>
    <th>Select</th><th>Name</th><th>State</th><th>UUID</th><th>Sender URI</th>
  </tr>
%for receiver in active:
  <tr>
    <td><input type="radio" name="Master" value="{{receiver[2]}}"/></td>
    <td>{{receiver[0]}}</td>
    <td>{{receiver[1]}}</td>
    <td>{{receiver[2]}}</td>
    <td>{{receiver[3]}}</td>
  </tr>
%end
</table>
<p></p>
<table>
    <caption>Choose Associates</caption>
  <tr>
    <th>Select</th><th>Name</th><th>State</th><th>UUID</th>
  </tr>
%for receiver in others:
  <tr>
    <td><input type="checkbox" name="Assoc" value="{{receiver[2]}}"/></td>
    <td>{{receiver[0]}}</td>
    <td>{{receiver[1]}}</td>
    <td>{{receiver[2]}}</td>
  </tr>
%end
</table>
<p></p>

<input type="submit">
</form>

%end

%include('footer.tpl')
